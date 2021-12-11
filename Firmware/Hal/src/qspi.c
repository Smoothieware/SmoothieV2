#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"

#include "Hal_pin.h"

#include <stdbool.h>
#include <stdio.h>

#define QSPI_CLK_ENABLE()          __HAL_RCC_QSPI_CLK_ENABLE()
#define QSPI_CLK_DISABLE()         __HAL_RCC_QSPI_CLK_DISABLE()
#define QSPI_CS_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOB_CLK_ENABLE()
#define QSPI_CLK_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define QSPI_D0_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
#ifdef BOARD_DEVEBOX
#define QSPI_D1_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
#else
#define QSPI_D1_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOF_CLK_ENABLE()
#endif
#define QSPI_D2_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOE_CLK_ENABLE()
#define QSPI_D3_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
#define QSPI_MDMA_CLK_ENABLE()      __HAL_RCC_MDMA_CLK_ENABLE()

#define QSPI_FORCE_RESET()         __HAL_RCC_QSPI_FORCE_RESET()
#define QSPI_RELEASE_RESET()       __HAL_RCC_QSPI_RELEASE_RESET()

/* Definition for QSPI Pins */
#define QSPI_CS_PIN                GPIO_PIN_6
#define QSPI_CS_GPIO_PORT          GPIOB
#define QSPI_CLK_PIN               GPIO_PIN_2
#define QSPI_CLK_GPIO_PORT         GPIOB
#define QSPI_D0_PIN                GPIO_PIN_11
#define QSPI_D0_GPIO_PORT          GPIOD
#ifdef BOARD_DEVEBOX
#define QSPI_D1_PIN                GPIO_PIN_12
#define QSPI_D1_GPIO_PORT          GPIOD
#define QSPI_D1_AF                 GPIO_AF9_QUADSPI
#else
#define QSPI_D1_PIN                GPIO_PIN_9
#define QSPI_D1_GPIO_PORT          GPIOF
#define QSPI_D1_AF                 GPIO_AF10_QUADSPI
#endif
#define QSPI_D2_PIN                GPIO_PIN_2
#define QSPI_D2_GPIO_PORT          GPIOE
#define QSPI_D3_PIN                GPIO_PIN_13
#define QSPI_D3_GPIO_PORT          GPIOD

/* Definition for QSPI DMA */
#define QSPI_DMA_INSTANCE          DMA2_Stream7
#define QSPI_DMA_REQUEST           $QSPI_DMA_REQUEST$
#define QSPI_DMA_IRQ               DMA2_Stream7_IRQn
#define QSPI_DMA_IRQ_HANDLER       DMA2_Stream7_IRQHandler

/* MT25TL01GHBA8ESF Micron memory */
/* Size of the flash */
#define QSPI_FLASH_SIZE                      25
#define QSPI_PAGE_SIZE                       256

/* Reset Operations */
#define RESET_ENABLE_CMD                     0x66
#define RESET_MEMORY_CMD                     0x99

/* Identification Operations */
#define READ_ID_CMD                          0x9E
#define READ_ID_CMD2                         0x9F
#define MULTIPLE_IO_READ_ID_CMD              0xAF
#define READ_SERIAL_FLASH_DISCO_PARAM_CMD    0x5A

/* Read Operations */
#define READ_CMD                             0x03
#define READ_4_BYTE_ADDR_CMD                 0x13

#define FAST_READ_CMD                        0x0B
#define FAST_READ_DTR_CMD                    0x0D
#define FAST_READ_4_BYTE_ADDR_CMD            0x0C

#define DUAL_OUT_FAST_READ_CMD               0x3B
#define DUAL_OUT_FAST_READ_DTR_CMD           0x3D
#define DUAL_OUT_FAST_READ_4_BYTE_ADDR_CMD   0x3C

#define DUAL_INOUT_FAST_READ_CMD             0xBB
#define DUAL_INOUT_FAST_READ_DTR_CMD         0xBD
#define DUAL_INOUT_FAST_READ_4_BYTE_ADDR_CMD 0xBC

#define QUAD_OUT_FAST_READ_CMD               0x6B
#define QUAD_OUT_FAST_READ_DTR_CMD           0x6D
#define QUAD_OUT_FAST_READ_4_BYTE_ADDR_CMD   0x6C

#define QUAD_INOUT_FAST_READ_CMD             0xEB
#define QUAD_INOUT_FAST_READ_DTR_CMD         0xED
#define QUAD_INOUT_FAST_READ_4_BYTE_ADDR_CMD 0xEC

/* Write Operations */
#define WRITE_ENABLE_CMD                     0x06
#define WRITE_DISABLE_CMD                    0x04

/* Register Operations */
#define READ_STATUS_REG_CMD                  0x05
#define WRITE_STATUS_REG_CMD                 0x01

#define READ_LOCK_REG_CMD                    0xE8
#define WRITE_LOCK_REG_CMD                   0xE5

#define READ_FLAG_STATUS_REG_CMD             0x70
#define CLEAR_FLAG_STATUS_REG_CMD            0x50

#define READ_NONVOL_CFG_REG_CMD              0xB5
#define WRITE_NONVOL_CFG_REG_CMD             0xB1

#define READ_VOL_CFG_REG_CMD                 0x85
#define WRITE_VOL_CFG_REG_CMD                0x81

#define READ_ENHANCED_VOL_CFG_REG_CMD        0x65
#define WRITE_ENHANCED_VOL_CFG_REG_CMD       0x61

#define READ_EXT_ADDR_REG_CMD                0xC8
#define WRITE_EXT_ADDR_REG_CMD               0xC5

/* Program Operations */
#define PAGE_PROG_CMD                        0x02
#define PAGE_PROG_4_BYTE_ADDR_CMD            0x12

#define DUAL_IN_FAST_PROG_CMD                0xA2
#define EXT_DUAL_IN_FAST_PROG_CMD            0xD2

#define QUAD_IN_FAST_PROG_CMD                0x32
#define EXT_QUAD_IN_FAST_PROG_CMD            0x12 /*0x38*/
#define QUAD_IN_FAST_PROG_4_BYTE_ADDR_CMD    0x34

/* Erase Operations */
#define SUBSECTOR_ERASE_CMD                  0x20
#define SUBSECTOR_ERASE_4_BYTE_ADDR_CMD      0x21

#define SECTOR_ERASE_CMD                     0xD8
#define SECTOR_ERASE_4_BYTE_ADDR_CMD         0xDC

#define BULK_ERASE_CMD                       0xC7

#define PROG_ERASE_RESUME_CMD                0x7A
#define PROG_ERASE_SUSPEND_CMD               0x75

/* One-Time Programmable Operations */
#define READ_OTP_ARRAY_CMD                   0x4B
#define PROG_OTP_ARRAY_CMD                   0x42

/* 4-byte Address Mode Operations */
#define ENTER_4_BYTE_ADDR_MODE_CMD           0xB7
#define EXIT_4_BYTE_ADDR_MODE_CMD            0xE9

/* Quad Operations */
#define ENTER_QUAD_CMD                       0x35
#define EXIT_QUAD_CMD                        0xF5

/* Default dummy clocks cycles */
#define DUMMY_CLOCK_CYCLES_READ              8
#define DUMMY_CLOCK_CYCLES_READ_QUAD         8

#define DUMMY_CLOCK_CYCLES_READ_DTR          6
#define DUMMY_CLOCK_CYCLES_READ_QUAD_DTR     8

static QSPI_HandleTypeDef QSPIHandle;
static volatile uint8_t CmdCplt, TxCplt, StatusMatch;

/**
  * @brief QSPI MSP Initialization
  *        This function configures the hardware resources used in this example:
  *           - Peripheral's clock enable
  *           - Peripheral's GPIO Configuration
  *           - DMA configuration for requests by peripheral
  *           - NVIC configuration for DMA and QSPI interrupts
  * @param hqspi: QSPI handle pointer
  * @retval None
  */
void HAL_QSPI_MspInit(QSPI_HandleTypeDef *hqspi)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    static MDMA_HandleTypeDef hmdma;

    /*##-1- Enable peripherals and GPIO Clocks #################################*/
    /* Enable the QuadSPI memory interface clock */
    QSPI_CLK_ENABLE();
    /* Reset the QuadSPI memory interface */
    QSPI_FORCE_RESET();
    QSPI_RELEASE_RESET();
    /* Enable GPIO clocks */
    QSPI_CS_GPIO_CLK_ENABLE();
    QSPI_CLK_GPIO_CLK_ENABLE();
    QSPI_D0_GPIO_CLK_ENABLE();
    QSPI_D1_GPIO_CLK_ENABLE();
    QSPI_D2_GPIO_CLK_ENABLE();
    QSPI_D3_GPIO_CLK_ENABLE();
    /* Enable DMA clock */
    QSPI_MDMA_CLK_ENABLE();

    /*##-2- Configure peripheral GPIO ##########################################*/
    /* QSPI CS GPIO pin configuration  */
    GPIO_InitStruct.Pin       = QSPI_CS_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(QSPI_CS_GPIO_PORT, &GPIO_InitStruct);

    /* QSPI CLK GPIO pin configuration  */
    GPIO_InitStruct.Pin       = QSPI_CLK_PIN;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(QSPI_CLK_GPIO_PORT, &GPIO_InitStruct);

    /* QSPI D0 GPIO pin configuration  */
    GPIO_InitStruct.Pin       = QSPI_D0_PIN;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(QSPI_D0_GPIO_PORT, &GPIO_InitStruct);

    /* QSPI D1 GPIO pin configuration  */
    GPIO_InitStruct.Pin       = QSPI_D1_PIN;
    GPIO_InitStruct.Alternate = QSPI_D1_AF;
    HAL_GPIO_Init(QSPI_D1_GPIO_PORT, &GPIO_InitStruct);

    /* QSPI D2 GPIO pin configuration  */
    GPIO_InitStruct.Pin       = QSPI_D2_PIN;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(QSPI_D2_GPIO_PORT, &GPIO_InitStruct);

    /* QSPI D3 GPIO pin configuration  */
    GPIO_InitStruct.Pin       = QSPI_D3_PIN;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(QSPI_D3_GPIO_PORT, &GPIO_InitStruct);

    // mark pins as allocated
    allocate_hal_pin(QSPI_CS_GPIO_PORT, QSPI_CS_PIN);
    allocate_hal_pin(QSPI_CLK_GPIO_PORT, QSPI_CLK_PIN);
    allocate_hal_pin(QSPI_D0_GPIO_PORT, QSPI_D0_PIN);
    allocate_hal_pin(QSPI_D1_GPIO_PORT, QSPI_D1_PIN);
    allocate_hal_pin(QSPI_D2_GPIO_PORT, QSPI_D2_PIN);
    allocate_hal_pin(QSPI_D3_GPIO_PORT, QSPI_D3_PIN);

    /*##-3- Configure the NVIC for QSPI #########################################*/
    /* NVIC configuration for QSPI interrupt */
    NVIC_SetPriority(QUADSPI_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
    NVIC_EnableIRQ(QUADSPI_IRQn);

    /*##-4- Configure the DMA channel ###########################################*/
    /* QSPI DMA channel configuration */
    hmdma.Init.Request = MDMA_REQUEST_QUADSPI_FIFO_TH;
    hmdma.Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER;
    hmdma.Init.Priority = MDMA_PRIORITY_HIGH;
    hmdma.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;

    hmdma.Init.SourceInc = MDMA_SRC_INC_BYTE;
    hmdma.Init.DestinationInc = MDMA_DEST_INC_DISABLE;
    hmdma.Init.SourceDataSize = MDMA_SRC_DATASIZE_BYTE;
    hmdma.Init.DestDataSize = MDMA_DEST_DATASIZE_BYTE;
    hmdma.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
    hmdma.Init.BufferTransferLength = 4;
    hmdma.Init.SourceBurst = MDMA_SOURCE_BURST_SINGLE;
    hmdma.Init.DestBurst = MDMA_DEST_BURST_SINGLE;

    hmdma.Init.SourceBlockAddressOffset = 0;
    hmdma.Init.DestBlockAddressOffset = 0;

    hmdma.Instance = MDMA_Channel1;

    /* Associate the DMA handle */
    __HAL_LINKDMA(hqspi, hmdma, hmdma);

    /* DeInitialize the MDMA Stream */
    HAL_MDMA_DeInit(&hmdma);
    /* Initialize the MDMA stream */
    HAL_MDMA_Init(&hmdma);

    /* Enable and set QuadSPI interrupt to the lowest priority */
    NVIC_SetPriority(MDMA_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(MDMA_IRQn);
}

/**
  * @brief QSPI MSP De-Initialization
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  *          - Revert GPIO, DMA and NVIC configuration to their default state
  * @param hqspi: QSPI handle pointer
  * @retval None
  */
void HAL_QSPI_MspDeInit(QSPI_HandleTypeDef *hqspi)
{
    static DMA_HandleTypeDef hdma;

    /*##-1- Disable the NVIC for QSPI and DMA ##################################*/
    NVIC_DisableIRQ(QSPI_DMA_IRQ);
    NVIC_DisableIRQ(QUADSPI_IRQn);

    /*##-2- Disable peripherals ################################################*/
    /* De-configure DMA channel */
    hdma.Instance = QSPI_DMA_INSTANCE;
    HAL_DMA_DeInit(&hdma);
    /* De-Configure QSPI pins */
    HAL_GPIO_DeInit(QSPI_CS_GPIO_PORT, QSPI_CS_PIN);
    HAL_GPIO_DeInit(QSPI_CLK_GPIO_PORT, QSPI_CLK_PIN);
    HAL_GPIO_DeInit(QSPI_D0_GPIO_PORT, QSPI_D0_PIN);
    HAL_GPIO_DeInit(QSPI_D1_GPIO_PORT, QSPI_D1_PIN);
    HAL_GPIO_DeInit(QSPI_D2_GPIO_PORT, QSPI_D2_PIN);
    HAL_GPIO_DeInit(QSPI_D3_GPIO_PORT, QSPI_D3_PIN);

    /*##-3- Reset peripherals ##################################################*/
    /* Reset the QuadSPI memory interface */
    QSPI_FORCE_RESET();
    QSPI_RELEASE_RESET();

    /* Disable the QuadSPI memory interface clock */
    QSPI_CLK_DISABLE();
}

/**
  * @brief  This function handles QUADSPI interrupt request.
  * @param  None
  * @retval None
  */
void QUADSPI_IRQHandler(void)
{
    HAL_QSPI_IRQHandler(&QSPIHandle);
}


/**
  * @brief  This function handles QUADSPI DMA interrupt request.
  * @param  None
  * @retval None
  */

void MDMA_IRQHandler(void)
{
    /* Check the interrupt and clear flag */
    HAL_MDMA_IRQHandler(QSPIHandle.hmdma);
}

/**
  * @brief  This function send a Write Enable and wait it is effective.
  * @param  hqspi: QSPI handle
  * @retval None
  */
static bool QSPI_WriteEnable(QSPI_HandleTypeDef *hqspi)
{
    QSPI_CommandTypeDef     sCommand;
    QSPI_AutoPollingTypeDef sConfig;

    /* Enable write operations ------------------------------------------ */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = WRITE_ENABLE_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_NONE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    /* Configure automatic polling mode to wait for write enabling ---- */
    sConfig.Match           = 0x02;
    sConfig.Mask            = 0x02;
    sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
    sConfig.StatusBytesSize = 1;
    sConfig.Interval        = 0x10;
    sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    sCommand.Instruction    = READ_STATUS_REG_CMD;
    sCommand.DataMode       = QSPI_DATA_1_LINE;

    if (HAL_QSPI_AutoPolling(&QSPIHandle, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    return true;
}

/**
  * @brief  This function read the SR of the memory and wait the EOP.
  * @param  hqspi: QSPI handle
  * @retval None
  */
static bool QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *hqspi)
{
    QSPI_CommandTypeDef     sCommand;
    QSPI_AutoPollingTypeDef sConfig;

    /* Configure automatic polling mode to wait for memory ready ------ */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = READ_STATUS_REG_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_1_LINE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    sConfig.Match           = 0x00;
    sConfig.Mask            = 0x01;
    sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
    sConfig.StatusBytesSize = 1;
    sConfig.Interval        = 0x10;
    sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_QSPI_AutoPolling_IT(&QSPIHandle, &sCommand, &sConfig) != HAL_OK) {
        return false;
    }

    return true;
}

/**
  * @brief  This function configure the dummy cycles on memory side.
  * @param  hqspi: QSPI handle
  * @retval None
  */
static bool QSPI_DummyCyclesCfg(QSPI_HandleTypeDef *hqspi)
{
    QSPI_CommandTypeDef sCommand;
    uint8_t reg;

    /* Read Volatile Configuration register --------------------------- */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = READ_VOL_CFG_REG_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_1_LINE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    sCommand.NbData            = 1;

    if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    if (HAL_QSPI_Receive(&QSPIHandle, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    /* Enable write operations ---------------------------------------- */
    QSPI_WriteEnable(&QSPIHandle);

    /* Write Volatile Configuration register (with new dummy cycles) -- */
    sCommand.Instruction = WRITE_VOL_CFG_REG_CMD;
    MODIFY_REG(reg, 0xF0, (DUMMY_CLOCK_CYCLES_READ_QUAD << POSITION_VAL(0xF0)));

    if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    if (HAL_QSPI_Transmit(&QSPIHandle, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return false;
    }

    return true;
}

bool qspi_init()
{
#ifdef BOARD_NUCLEO
    printf("WARNING: qspi_init: NUCLEO does not support qspi\n");
    return false;
#endif

    /* Initialize QuadSPI ------------------------------------------------------ */
    QSPIHandle.Instance = QUADSPI;
    HAL_QSPI_DeInit(&QSPIHandle);

    QSPIHandle.Init.ClockPrescaler     = 1;
    QSPIHandle.Init.FifoThreshold      = 4;
    QSPIHandle.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    QSPIHandle.Init.FlashSize          = QSPI_FLASH_SIZE;
    QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
    QSPIHandle.Init.ClockMode          = QSPI_CLOCK_MODE_0;
    QSPIHandle.Init.FlashID            = QSPI_FLASH_ID_1;
    QSPIHandle.Init.DualFlash          = QSPI_DUALFLASH_DISABLE;

    if (HAL_QSPI_Init(&QSPIHandle) != HAL_OK) {
        printf("ERROR: qspi_init: Init failed\n");
        return false;
    }

    return true;
}

// map the qspi to memory at 0x90000000
bool qspi_mount()
{
    if(!qspi_init()) {
        printf("ERROR: qspi_init failed\n");
        return false;
    }

    QSPI_CommandTypeDef      sCommand;
    QSPI_MemoryMappedTypeDef sMemMappedCfg;

    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
    sCommand.DataMode          = QSPI_DATA_4_LINES;
    /* Reading Sequence ------------------------------------------------ */
    sCommand.Instruction = QUAD_OUT_FAST_READ_CMD;
    sCommand.DummyCycles = DUMMY_CLOCK_CYCLES_READ_QUAD;

    /* Configure Volatile Configuration register (with new dummy cycles) */
    QSPI_DummyCyclesCfg(&QSPIHandle);

    sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;

    if (HAL_QSPI_MemoryMapped(&QSPIHandle, &sCommand, &sMemMappedCfg) != HAL_OK) {
        printf("ERROR: qspi_flash: MemoryMapped failed\n");
        return false;
    }

    return true;
}

// flash the contents of the file into QSPI
static uint8_t flash_buffer[QSPI_PAGE_SIZE] __attribute__((aligned(4))) ;
bool qspi_flash(const char* fn)
{
    if(!qspi_init()) {
        printf("ERROR: qspi_init failed\n");
        return false;
    }

    FIL fp;     /* File object */

    printf("DEBUG: qspi_flash: Opening %s from SD Card...\n", fn);

    FRESULT rc = f_open(&fp, fn, FA_READ);
    if (rc) {
        printf("ERROR: qspi_flash: no %s found\n", fn);
        return false;
    }

    printf("DEBUG: qspi_flash: Flashing contents of file...\n");

    QSPI_CommandTypeDef      sCommand;
    __IO uint32_t qspi_addr = 0;
    __IO uint8_t step = 0;
    uint32_t max_size = f_size(&fp);
    __IO uint32_t size = 0;
    __IO int erase_size = max_size;
    __IO bool eraseing = true;
    __IO uint32_t cnt = 0;

    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    // state machine
    while(1) {
        switch(step) {
            case 0:
                CmdCplt = 0;

                /* Enable write operations ------------------------------------------- */
                QSPI_WriteEnable(&QSPIHandle);

                /* Erasing Sequence -------------------------------------------------- */
                sCommand.Instruction = SECTOR_ERASE_CMD;
                sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
                sCommand.Address     = qspi_addr;
                sCommand.DataMode    = QSPI_DATA_NONE;
                sCommand.DummyCycles = 0;
                // erases 64k
                if (HAL_QSPI_Command_IT(&QSPIHandle, &sCommand) != HAL_OK) {
                    printf("ERROR: qspi_flash: Erase failed\n");
                    f_close(&fp);
                    return false;
                }

                step++;
                break;

            case 1:
                if(CmdCplt != 0) {
                    CmdCplt = 0;
                    StatusMatch = 0;

                    /* Configure automatic polling mode to wait for end of erase ------- */
                    QSPI_AutoPollingMemReady(&QSPIHandle);

                    step++;
                }
                break;

            case 2:
                if(StatusMatch != 0) {
                    StatusMatch = 0;
                    TxCplt = 0;

                    // see if erase complete
                    if(eraseing) {
                        erase_size -= (64 * 1024);
                        if(erase_size > 0) {
                            step = 0;
                            qspi_addr += (64 * 1024);
                            continue;
                        } else {
                            eraseing = 0;
                            qspi_addr = 0;
                        }
                    }

                    // read next page of data to flash
                    UINT br;
                    rc = f_read(&fp, (void *)flash_buffer, sizeof(flash_buffer), &br);
                    if (rc != FR_OK) {
                        printf("ERROR: qspi_flash: read failed\n");
                        f_close(&fp);
                        return false;
                    }

                    size = br;
                    if(br == 0) {
                        step = 4;
                        StatusMatch = 1;
                        break;
                    }

                    // make sure cache is flushed to RAM so the DMA can read the correct data
                    uint32_t cache_addr = (uint32_t)&flash_buffer[0];
                    SCB_CleanDCache_by_Addr((uint32_t *)((uint32_t)cache_addr & ~0x1f),
                                            ((uint32_t)((uint8_t *)cache_addr + sizeof(flash_buffer) + 0x1f) & ~0x1f) -
                                            ((uint32_t)cache_addr & ~0x1f));

                    /* Enable write operations ----------------------------------------- */
                    QSPI_WriteEnable(&QSPIHandle);

                    /* Writing Sequence ------------------------------------------------ */
                    sCommand.Instruction = QUAD_IN_FAST_PROG_CMD;
                    sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
                    sCommand.Address     = qspi_addr;
                    sCommand.DataMode    = QSPI_DATA_4_LINES;
                    sCommand.NbData      = size;

                    if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
                        printf("ERROR: qspi_flash: command failed\n");
                        f_close(&fp);
                        return false;
                    }

                    if (HAL_QSPI_Transmit_DMA(&QSPIHandle, flash_buffer) != HAL_OK) {
                        printf("ERROR: qspi_flash: transmit failed\n");
                        f_close(&fp);
                        return false;
                    }

                    step++;

                }
                break;

            case 3:
                if(TxCplt != 0) {
                    TxCplt = 0;
                    StatusMatch = 0;

                    /* Configure automatic polling mode to wait for write to complete ----- */
                    QSPI_AutoPollingMemReady(&QSPIHandle);

                    step++;
                }
                break;

            case 4:
                if(StatusMatch != 0) {
                    cnt += size;
                    // Check if a new page needs to be written
                    if (cnt < max_size) {
                        qspi_addr += size;
                        step = 2;

                    } else {
                        f_close(&fp);
                        return qspi_mount();
                    }
                }
                break;

            default :
                return false;
        }
        vTaskDelay(0);
    }
}

/**
  * @brief  Command completed callbacks.
  * @param  hqspi: QSPI handle
  * @retval None
  */
void HAL_QSPI_CmdCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    CmdCplt++;
}

/**
  * @brief  Tx Transfer completed callbacks.
  * @param  hqspi: QSPI handle
  * @retval None
  */
void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    TxCplt++;
}

/**
  * @brief  Status Match callbacks
  * @param  hqspi: QSPI handle
  * @retval None
  */
void HAL_QSPI_StatusMatchCallback(QSPI_HandleTypeDef *hqspi)
{
    StatusMatch++;
}
