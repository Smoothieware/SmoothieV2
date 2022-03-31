// Only thermistor inputs are supersampled
// TODO allow non thermistor instances to be created

#include "Adc.h"
#include "SlowTicker.h"

#include <string>
#include <cctype>
#include <algorithm>
#include <string.h>
#include <set>

#include "FreeRTOS.h"
#include "task.h"

#include "stm32h7xx_hal.h"
#include "Hal_pin.h"
#include "StringUtils.h"

Adc *Adc::instances[Adc::num_channels];
std::set<uint16_t> Adc::allocated_channels;
bool Adc::running;

// make sure it is aligned on 32byte boundary for cache coherency, need to allocate potentially max size
// num_samples (8) samples per num_channels (8) channels
// We add 32 bytes just to make sure nothing else could share this area as we invalidate the cache after DMA
ALIGN_32BYTES(static __IO uint16_t aADCxConvertedData[(Adc::num_samples * Adc::num_channels) + 32]);

// this will be the actual size of the data based on the number of ADC channels actually in use
static size_t adc_data_size;

Adc::Adc(const char *nm)
{
    // decode name eg ADC1_0
    name.assign(nm);
    if(name.size() != 6 || stringutils::toUpper(name.substr(0, 3)) != "ADC" || name[4] != '_') {
        printf("ERROR: illegal ADC name: %s\n", nm);
        return;
    }

    if(name[3] != '1') {
        printf("ERROR: Illegal ADC only ADC1 supported: %s\n", nm);
        return;
    }

    channel = strtol(name.substr(5).c_str(), nullptr, 10);
    if(channel >= num_channels) {
        printf("ERROR: Illegal ADC channel: %d\n", channel);
        return;
    }

    if(allocated_channels.count(channel) == 0) {
        instances[channel] = this;
        valid = true;
        allocated_channels.insert(channel);
    } else {
        printf("ERROR: ADC channel %d is already in use\n", channel);
    }
}

Adc::~Adc()
{
    if(!valid) return;

    // remove from instances array
    taskENTER_CRITICAL();
    allocated_channels.erase(channel);
    instances[channel] = nullptr;
    taskEXIT_CRITICAL();
}

std::string Adc::to_string() const
{
    return this->name;
}

#define ADCx                            ADC1
#define ADCx_CLK_ENABLE()               __HAL_RCC_ADC12_CLK_ENABLE()
#define ADCx_CHANNEL_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()

#define DMAx_CHANNELx_CLK_ENABLE()      __HAL_RCC_DMA1_CLK_ENABLE()

#define ADCx_FORCE_RESET()              __HAL_RCC_ADC12_FORCE_RESET()
#define ADCx_RELEASE_RESET()            __HAL_RCC_ADC12_RELEASE_RESET()

// Definitions for all allowed ADC channels we use
#define ADCx_CHANNEL_PIN_CLK_ENABLE()   __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();__HAL_RCC_GPIOF_CLK_ENABLE();

#define ADC_RESOLUTION ADC_RESOLUTION_16B

/*
ADC1_INP0       PA0_C (with switch closed can read on PA0)
ADC1_INP2       PF11
ADC1_INP6       PF12
ADC1_INP9       PB0
ADC1_INP12      PC2
ADC1_INP13      PC3
ADC1_INP15      PA3
optionally...
ADC2_INP2       PF13
ADC2_INP6       PF14
*/

static struct {GPIO_TypeDef* port; uint32_t pin;} adcpinlut[] = {
    {GPIOA, GPIO_PIN_0},
    {GPIOF, GPIO_PIN_11},
    {GPIOF, GPIO_PIN_12},
    {GPIOB, GPIO_PIN_0},
    {GPIOC, GPIO_PIN_2},
    {GPIOC, GPIO_PIN_3},
    {GPIOA, GPIO_PIN_3},
};

static uint32_t adc_channel_lut[] = {
    ADC_CHANNEL_0,
    ADC_CHANNEL_2,
    ADC_CHANNEL_6,
    ADC_CHANNEL_9,
    ADC_CHANNEL_12,
    ADC_CHANNEL_13,
    ADC_CHANNEL_15,
};

static uint32_t adc_rank_lut[] = {
    ADC_REGULAR_RANK_1,
    ADC_REGULAR_RANK_2,
    ADC_REGULAR_RANK_3,
    ADC_REGULAR_RANK_4,
    ADC_REGULAR_RANK_5,
    ADC_REGULAR_RANK_6,
    ADC_REGULAR_RANK_7,
};

static ADC_HandleTypeDef AdcHandle;

bool Adc::post_config_setup()
{
    printf("DEBUG: ADC post config setup\n");

    // This is not called until after all modules have been configged and we know how many ADC channels we need
    int nc = allocated_channels.size();
    if(nc == 0) {
        printf("WARNING: ADC No channels allocated\n");
        return false;
    }

    /* ### - 1 - Initialize ADC peripheral #################################### */
    AdcHandle.Instance          = ADCx;
    HAL_ADC_DeInit(&AdcHandle);

    AdcHandle.Init.ClockPrescaler           = ADC_CLOCK_ASYNC_DIV6;          /* Asynchronous clock mode, input ADC clock divided by 6*/
    AdcHandle.Init.Resolution               = ADC_RESOLUTION;                /* 16-bit resolution for converted data */
    AdcHandle.Init.ScanConvMode             = ENABLE;                        // Sequencer enabled
    AdcHandle.Init.EOCSelection             = ADC_EOC_SEQ_CONV;              /* EOC flag picked-up to indicate conversion end */
    AdcHandle.Init.LowPowerAutoWait         = DISABLE;                       /* Auto-delayed conversion feature disabled */
    AdcHandle.Init.ContinuousConvMode       = ENABLE;                        /* Continuous mode enabled (automatic conversion restart after each conversion) */
    AdcHandle.Init.NbrOfConversion          = nc;
    AdcHandle.Init.DiscontinuousConvMode    = DISABLE;                       /* Parameter discarded because sequencer is disabled */
    AdcHandle.Init.NbrOfDiscConversion      = 1;                             /* Parameter discarded because sequencer is disabled */
    AdcHandle.Init.ExternalTrigConv         = ADC_SOFTWARE_START;            /* Software start to trig the 1st conversion manually, without external event */
    AdcHandle.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_NONE; /* Parameter discarded because software trigger chosen */
    AdcHandle.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR; /* ADC DMA circular requested */
    AdcHandle.Init.Overrun                  = ADC_OVR_DATA_OVERWRITTEN;      /* DR register is overwritten with the last conversion result in case of overrun */
    AdcHandle.Init.OversamplingMode         = DISABLE;                       /* No oversampling */
    /* Initialize ADC peripheral according to the passed parameters */
    if (HAL_ADC_Init(&AdcHandle) != HAL_OK) {
        printf("ERROR: ADC1 ADC_Init failed\n");
        return false;
    }

    /* ### - 2 - Start calibration ############################################ */
    if (HAL_ADCEx_Calibration_Start(&AdcHandle, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        printf("ERROR: ADC1 Calibration failed\n");
        return false;
    }

    /* ### - 3 - Channel configuration ######################################## */
    // For each channel
    int rank = 0;
    for(uint16_t c : Adc::allocated_channels) {
        ADC_ChannelConfTypeDef sConfig{0};
        sConfig.Channel      = adc_channel_lut[c];                /* Sampled channel number */
        sConfig.Rank         = adc_rank_lut[rank++];          /* Rank of sampled channel number ADCx_CHANNEL */
        sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;   /* Sampling time (number of clock cycles unit) */
        sConfig.SingleDiff   = ADC_SINGLE_ENDED;            /* Single-ended input channel */
        sConfig.OffsetNumber = ADC_OFFSET_NONE;             /* No offset subtraction */
        sConfig.Offset = 0;                                 /* Parameter discarded because offset correction is disabled */
        if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK) {
            printf("ERROR: ADC1 ConfigChannel %d failed\n", c);
            return false;
        }
    }

    // num_samples (8) samples per channel
    adc_data_size = nc * num_samples; // actual data size

    return true;
}

// mainly for testing
bool Adc::deinit()
{
    HAL_ADC_DeInit(&AdcHandle);
    return true;
}

bool Adc::start()
{
    running = true;
    if (HAL_ADC_Start_DMA(&AdcHandle, (uint32_t *)aADCxConvertedData, adc_data_size) != HAL_OK) {
        printf("ERROR: ADC1 Start DMA failed\n");
        return false;
    }

    return true;
}

bool Adc::stop()
{
    running = false;
    HAL_ADC_Stop_DMA(&AdcHandle);
    return true;
}

//#define MEDIAN
#ifdef MEDIAN
static void split(uint16_t data[], unsigned int n, uint16_t x, unsigned int& i, unsigned int& j)
{
    do {
        while (data[i] < x) i++;
        while (x < data[j]) j--;

        if (i <= j) {
            uint16_t ii = data[i];
            data[i] = data[j];
            data[j] = ii;
            i++; j--;
        }
    } while (i <= j);
}

// C.A.R. Hoare's Quick Median
static unsigned int quick_median(uint16_t data[], unsigned int n)
{
    unsigned int l = 0, r = n - 1, k = n / 2;
    while (l < r) {
        uint16_t x = data[k];
        unsigned int i = l, j = r;
        split(data, n, x, i, j);
        if (j < k) l = i;
        if (k < i) r = j;
    }
    return k;
}
#else
static uint32_t average(uint16_t *buf, int n)
{
    float acc= 0.0F;
    for (int i = 0; i < n; ++i) {
        acc += buf[i];
    }
    return roundf(acc/n);
}
#endif

//#define ADC_TIMEIT
#ifdef ADC_TIMEIT
#include "benchmark_timer.h"
static uint32_t st, elt;
uint32_t adc_get_time()
{
    return benchmark_timer_as_us(elt);
}
#endif

// This will take 154 us per sample with current settings
// for 8 samples on a channel this is 1.2ms per channel
// with 2 channels this gets called about every 1.2ms (or 2.4ms for complete read)
// If half is true then only the first half has been captured
void Adc::sample_isr(bool half)
{
    if(!running) return;

    // I think this means we have 8 samples from each channel interleaved
    int n = allocated_channels.size();
    int o = 0;
    int ns2 = num_samples/2;
    uint16_t dataADC[ns2];
    int off= half ? 0 : ns2;
    for(uint16_t c : allocated_channels) {
        Adc *adc = getInstance(c);
        if(adc == nullptr || !adc->valid) continue; // not setup
        // pick it out of the array, only half the array is ready
        for(int i = 0; i < ns2; ++i) {
            dataADC[i] = aADCxConvertedData[((i+off) * n) + o];
        }
        memcpy(adc->sample_buffer+off, dataADC, sizeof(dataADC));
        ++o;
    }

    if(!half) {
        #ifdef ADC_TIMEIT
        // 2461us between calls
        elt= benchmark_timer_elapsed(st);
        st= benchmark_timer_start();
        #endif
        // calculate median for each buffer, avoids having to lock the buffer
        for(uint16_t c : allocated_channels) {
            Adc *adc = getInstance(c);
            if(adc == nullptr || !adc->valid) continue; // not setup
            #ifdef MEDIAN
            adc->last_sample= adc->sample_buffer[quick_median(adc->sample_buffer, num_samples)];
            #else
            adc->last_sample= average(adc->sample_buffer, num_samples);
            #endif
        }
    }
}

// gets called 20 times a second from an ISR or timer
uint32_t Adc::read()
{
    // returns the median value of the last 8 samples
    return last_sample;
}

uint16_t Adc::read_raw()
{
    // just return the ADC on the pin
    return last_sample;
}

float Adc::read_voltage()
{
    // just return the ADC on the pin
    uint16_t adc = last_sample;
    float v = 3.3F * ((float)adc / get_max_value());
    return v;
}

extern "C" void HAL_ADC3_MspInit(ADC_HandleTypeDef *hadc);
extern "C" void HAL_ADC3_MspDeInit(ADC_HandleTypeDef *hadc);

/**
* @brief  ADC MSP Init
* @param  hadc : ADC handle
* @retval None
*/
static DMA_HandleTypeDef DmaHandle;
extern "C" void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if(hadc->Instance == ADCx) {

        GPIO_InitTypeDef          GPIO_InitStruct{0};

        /*##-1- Enable peripherals and GPIO Clocks #################################*/
        /* Enable GPIO clock ****************************************/
        __HAL_RCC_GPIOA_CLK_ENABLE();
        /* ADC Periph clock enable */
        ADCx_CLK_ENABLE();
        /* ADC Periph interface clock configuration */
        __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_CLKP);
        /* Enable DMA clock */
        DMAx_CHANNELx_CLK_ENABLE();

        ADCx_CHANNEL_PIN_CLK_ENABLE();
        // For each channel, init the GPIOs
        for(uint16_t c : Adc::allocated_channels) {
            #ifdef BOARD_PRIME
            // check for special case of PA0_C, no need to config PA0 if on PA0_C
            if(adcpinlut[c].port == GPIOA && adcpinlut[c].pin == GPIO_PIN_0){
                HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA0, SYSCFG_SWITCH_PA0_OPEN);
                printf("DEBUG: ADC: PA0_C switch opened\n");
                continue;
            }
            #else
            // This allows us to connect to PA0 but read channel 0 (PA0_C)
            if(adcpinlut[c].port == GPIOA && adcpinlut[c].pin == GPIO_PIN_0){
                HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA0, SYSCFG_SWITCH_PA0_CLOSE);
                printf("DEBUG: ADC: PA0_C switch closed\n");
                continue;
            }
            #endif

            // lookup pin and port
            GPIO_InitStruct.Pin = adcpinlut[c].pin;
            GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            HAL_GPIO_Init(adcpinlut[c].port, &GPIO_InitStruct);
            allocate_hal_pin(adcpinlut[c].port, adcpinlut[c].pin);
        }

        /*##- 3- Configure DMA #####################################################*/

        /*********************** Configure DMA parameters ***************************/
        DmaHandle.Instance                 = DMA1_Stream1;
        DmaHandle.Init.Request             = DMA_REQUEST_ADC1;
        DmaHandle.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DmaHandle.Init.PeriphInc           = DMA_PINC_DISABLE;
        DmaHandle.Init.MemInc              = DMA_MINC_ENABLE;
        DmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        DmaHandle.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        DmaHandle.Init.Mode                = DMA_CIRCULAR;
        DmaHandle.Init.Priority            = DMA_PRIORITY_MEDIUM;
        /* Deinitialize  & Initialize the DMA for new transfer */
        HAL_DMA_DeInit(&DmaHandle);
        HAL_DMA_Init(&DmaHandle);

        /* Associate the DMA handle */
        __HAL_LINKDMA(hadc, DMA_Handle, DmaHandle);

        /* NVIC configuration for DMA Input data interrupt */
        NVIC_SetPriority(DMA1_Stream1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(DMA1_Stream1_IRQn);

    } else if(hadc->Instance == ADC3) {
        HAL_ADC3_MspInit(hadc);
    }

}

/**
  * @brief ADC MSP De-Initialization
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  *          - Revert GPIO to their default state
  * @  hadc: ADC handle pointer
  * @retval None
  */
extern "C" void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc)
{
    if(hadc->Instance == ADCx) {

        /*##-1- Reset peripherals ##################################################*/
        ADCx_FORCE_RESET();
        ADCx_RELEASE_RESET();
        /* ADC Periph clock disable
         (automatically reset all ADC instances of the ADC common group) */
        __HAL_RCC_ADC12_CLK_DISABLE();

        HAL_DMA_DeInit(&DmaHandle);

        /*##-2- Disable peripherals and GPIO Clocks ################################*/
        // For each channel, de init the GPIOs
        for(uint16_t c : Adc::allocated_channels) {
            HAL_GPIO_DeInit(adcpinlut[c].port, adcpinlut[c].pin);
            //deallocate_hal_pin(adcpinlut[c].port, adcpinlut[c].pin);
        }
        NVIC_DisableIRQ(DMA1_Stream1_IRQn);

    } else if(hadc->Instance == ADC3) {
        HAL_ADC3_MspDeInit(hadc);
    }
}

/**
* @brief  This function handles DMA1_Stream1_IRQHandler interrupt request.
* @param  None
* @retval None
*/
extern "C" void DMA1_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(AdcHandle.DMA_Handle);
}

extern "C" void HAL_ADC3_ConvCpltCallback(ADC_HandleTypeDef *hadc);
/**
  * @brief  Conversion DMA callback in non-blocking mode
  * @param  hadc: ADC handle
  * @retval None
  */
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance == ADCx) {
        // Invalidate Data Cache to get the updated content of the SRAM on the ADC converted data buffer
        SCB_InvalidateDCache_by_Addr((uint32_t *) aADCxConvertedData, adc_data_size*sizeof(uint16_t));
        Adc::sample_isr(false); // copy second half while first half is being filled

    }else{
        HAL_ADC3_ConvCpltCallback(hadc);
    }
}

/**
  * @brief  Conversion DMA half-transfer callback in non-blocking mode.
  * @param hadc ADC handle
  * @retval None
  */
extern "C" void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if(hadc->Instance != ADC1) return;

    // Invalidate Data Cache to get the updated content of the SRAM on the ADC converted data buffer
    SCB_InvalidateDCache_by_Addr((uint32_t *) aADCxConvertedData, adc_data_size*sizeof(uint16_t));
    Adc::sample_isr(true); // copy first half while second half is being filled
}
