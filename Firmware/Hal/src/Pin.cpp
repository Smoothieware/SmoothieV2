#include "Pin.h"

#include "stm32h7xx_hal.h"

#include <algorithm>
#include <cstring>
#include <bitset>
#include <set>

#include "StringUtils.h"

static std::bitset<16> allocated_pins[11];
static std::bitset<16> allocated_interrupt_pins;
static std::function<void(void)> interrupt_fncs[16] = {nullptr};

// Blacklisted pins which Pin() is not allowed to allocate
static std::set<std::string> black_listed {
#ifdef BOARD_DEVEBOX
    "PD11", "PD12", "PE2", "PD13", "PB6", "PB2",           // QSPI
#else
    "PD11", "PF9", "PE2", "PD13", "PB6", "PB2",           // QSPI
#endif
    "PG10", "PC8", "PC9", "PC10", "PC11", "PC12", "PD2", // SDCard
#if defined(BOARD_NUCLEO)
    "PA1", "PA2", "PA7", "PB13", "PC1", "PC4", "PC5", "PG11", "PG13", // Ethernet
#endif
#if defined(BOARD_PRIME)
    "PA1", "PA2", "PA7", "PG11", "PC1", "PC4", "PC5", "PG12", "PG13", // Ethernet
    "PB12", "PB13", "PB14", "PB15", // USB1
    "PA10", "PA11", "PA12",  // USB2
#endif
};

static uint16_t gpiopin2pin(uint16_t gpin)
{
    for (int i = 0; i < 16; ++i) {
        if((gpin >> i) & 1) {
            return i;
        }
    }
    return 16;
}

Pin::Pin()
{
    this->inverting = false;
    this->open_drain = false;
    this->valid = false;
    this->pullup = false;
    this->pulldown = false;
    this->interrupt = false;
}

Pin::Pin(const char *s)
{
    this->inverting = false;
    this->open_drain = false;
    this->valid = false;
    this->interrupt = false;
    from_string(s);
}

Pin::Pin(const char *s, TYPE_T t)
{
    this->inverting = false;
    this->valid = false;
    this->open_drain = false;
    this->interrupt = false;
    if(from_string(s)) {
        switch(t) {
            case AS_INPUT: as_input(); break;
            case AS_OUTPUT: as_output(); break;
        }
    }
}

Pin::~Pin()
{
    // TODO trouble is we copy pins so this would deallocate a used pin, see Robot actuator pins
    // deallocate it in the bitset, but leaves the physical port as it was
    // if(valid) {
    //     set_allocated(gpioport, gpiopin, false);
    // }

    if(valid && interrupt) {
        HAL_GPIO_DeInit((GPIO_TypeDef *)pport, ppin);
        interrupt_fncs[gpiopin] = nullptr;
        allocate_interrupt_pin(gpiopin, false);
        set_allocated(gpioport, gpiopin, false);
    }
}

// mainly for testing deallocate the gpio
bool Pin::deinit()
{
    HAL_GPIO_DeInit((GPIO_TypeDef *)pport, ppin);
    set_allocated(gpioport, gpiopin, false);
    valid= false;

    return true;
}

bool Pin::set_allocated(uint8_t port, uint8_t pin, bool set)
{
    port = toupper(port);
    if(port < 'A' || port > 'K' || pin >= 16) return false;
    uint8_t n= port - 'A';
    if(!set) {
        // deallocate it
        allocated_pins[n].reset(pin);
        return true;
    }

    if(!allocated_pins[n].test(pin)) {
        // if not set yet then set it
        allocated_pins[n].set(pin);
        return true;
    }

    // indicate it was already set
    return false;
}

bool Pin::is_allocated(uint8_t port, uint8_t pin)
{
    port = toupper(port);
    if(port < 'A' || port > 'K' || pin >= 16) return false;
    uint8_t n= port - 'A';
    return allocated_pins[n].test(pin);
}

bool Pin::allocate_interrupt_pin(uint8_t pin, bool set)
{
    if(pin >= allocated_interrupt_pins.size()) return false;

    if(!set) {
        // deallocate it
        allocated_interrupt_pins.reset(pin);
        return true;
    }

    if(!allocated_interrupt_pins[pin]) {
        // if not set yet then set it
        allocated_interrupt_pins.set(pin);
        return true;
    }

    // indicate it was already set
    return false;
}

extern "C" int allocate_hal_pin(void *port, uint16_t pin)
{
    uint8_t po = 0;
    if(port == GPIOA) po = 'A';
    else if(port == GPIOB) po = 'B';
    else if(port == GPIOC) po = 'C';
    else if(port == GPIOD) po = 'D';
    else if(port == GPIOE) po = 'E';
    else if(port == GPIOF) po = 'F';
    else if(port == GPIOG) po = 'G';
    else if(port == GPIOH) po = 'H';
    else if(port == GPIOI) po = 'I';
    else if(port == GPIOJ) po = 'J';
    else if(port == GPIOK) po = 'K';

    uint16_t pi= gpiopin2pin(pin);

    if(po > 0 && pi < 16) {
        if(Pin::set_allocated(po, pi, true)) {
            return 1;
        }
        printf("WARNING: P%c%d has already been allocated\n", po, pi);
        return 0;
    }

    printf("WARNING: invalid port/pin\n");
    return 0;
}

bool Pin::parse_pin(const std::string& value, char& port, uint16_t& pin, size_t& pos)
{
    if(value == "nc") return false;
    if(value.size() < 3 || toupper(value[0]) != 'P') return false;

    port = toupper(value[1]);
    if(port < 'A' || port > 'K') return false;

    pos = value.find_first_of("._", 2);
    if(pos == std::string::npos) pos = 1;
    pin = strtol(value.substr(pos + 1).c_str(), nullptr, 10);
    if(pin >= 16) return false;

    return true;
}

// Make a new pin object from a string
// Pins are defined for the STM32xxxx as PA6 or PA_6 or PA.6, second letter is A-K followed by a number 0-15
bool Pin::from_string(const std::string& value)
{
    valid = false;
    inverting = false;
    open_drain = false;

    char port = 0;
    uint16_t pin = 0;
    size_t pos;

    if(!parse_pin(value, port, pin, pos)) return false;

    {
        std::string s("P");
        s.append(1, port).append(std::to_string(pin));
        if(black_listed.count(s) != 0) {
            printf("ERROR: %s is a black listed pin and cannot be used\n", s.c_str());
            return false;
        }
    }

    if(!set_allocated(port, pin)) {
        printf("ERROR: P%c%d has already been allocated\n", port, pin);
        return false;
    }

    // now check for modifiers:-
    // ! = invert pin
    // o = set pin to open drain
    // ^ = set pin to pull up
    // v = set pin to pull down
    // - = set pin to no pull up or down
    // default to ^ for input pins
    this->pullup = true;
    this->pulldown = false;
    for(char c : value.substr(pos + 1)) {
        switch(c) {
            case '!':
                this->inverting = true;
                break;
            case 'o':
                this->open_drain = true;
                break;
            case '^':
                this->pullup = true;
                this->pulldown = false;
                break;
            case 'v':
                this->pulldown = true;
                this->pullup = false;
                break;
            case '-':
                this->pulldown = this->pullup = false; // clear both bits
                break;
        }
    }

    // save the gpio port and pin
    gpioport = port;
    gpiopin = pin;
    ppin = 1 << pin;

    switch(port) {
        case 'A': pport = (uint32_t *)GPIOA; __HAL_RCC_GPIOA_CLK_ENABLE(); break;
        case 'B': pport = (uint32_t *)GPIOB; __HAL_RCC_GPIOB_CLK_ENABLE(); break;
        case 'C': pport = (uint32_t *)GPIOC; __HAL_RCC_GPIOC_CLK_ENABLE(); break;
        case 'D': pport = (uint32_t *)GPIOD; __HAL_RCC_GPIOD_CLK_ENABLE(); break;
        case 'E': pport = (uint32_t *)GPIOE; __HAL_RCC_GPIOE_CLK_ENABLE(); break;
        case 'F': pport = (uint32_t *)GPIOF; __HAL_RCC_GPIOF_CLK_ENABLE(); break;
        case 'G': pport = (uint32_t *)GPIOG; __HAL_RCC_GPIOG_CLK_ENABLE(); break;
        case 'H': pport = (uint32_t *)GPIOH; __HAL_RCC_GPIOH_CLK_ENABLE(); break;
        case 'I': pport = (uint32_t *)GPIOI; __HAL_RCC_GPIOI_CLK_ENABLE(); break;
        case 'J': pport = (uint32_t *)GPIOJ; __HAL_RCC_GPIOJ_CLK_ENABLE(); break;
        case 'K': pport = (uint32_t *)GPIOK; __HAL_RCC_GPIOK_CLK_ENABLE(); break;
    }

    this->valid = true;
    return true;
}

std::string Pin::to_string() const
{
    if(valid) {
        std::string s("P");
        s.append(1, gpioport).append(std::to_string(gpiopin));

        if(open_drain) s.push_back('o');
        if(inverting) s.push_back('!');
        if(pullup) s.push_back('^');
        if(pulldown) s.push_back('v');
        if(get()) {
            s.append(":1");
        } else {
            s.append(":0");
        }
        return s;

    } else {
        return "nc";
    }
}

void Pin::toggle()
{
    set(!get());
}

bool Pin::as_output()
{
    if(!valid) return false;

    GPIO_InitTypeDef GPIO_InitStruct{0};
    GPIO_InitStruct.Mode = open_drain ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Pin = ppin;

    pullup = pulldown = false;

    HAL_GPIO_Init((GPIO_TypeDef *)pport, &GPIO_InitStruct);

    return true;
}

bool Pin::as_input()
{
    if(!valid) return false;

    GPIO_InitTypeDef GPIO_InitStruct{0};
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = pullup ? GPIO_PULLUP : pulldown ? GPIO_PULLDOWN : GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Pin = ppin;

    HAL_GPIO_Init((GPIO_TypeDef *)pport, &GPIO_InitStruct);

    return true;
}

static IRQn_Type getIRQ(uint16_t pin)
{
    switch(pin) {
        case GPIO_PIN_0: return EXTI0_IRQn;
        case GPIO_PIN_1: return EXTI1_IRQn;
        case GPIO_PIN_2: return EXTI2_IRQn;
        case GPIO_PIN_3: return EXTI3_IRQn;
        case GPIO_PIN_4: return EXTI4_IRQn;
        case GPIO_PIN_5:
        case GPIO_PIN_6:
        case GPIO_PIN_7:
        case GPIO_PIN_8:
        case GPIO_PIN_9: return EXTI9_5_IRQn;
        case GPIO_PIN_10:
        case GPIO_PIN_11:
        case GPIO_PIN_12:
        case GPIO_PIN_13:
        case GPIO_PIN_14:
        case GPIO_PIN_15: return EXTI15_10_IRQn;
    }
    return EXTI0_IRQn;
}

bool Pin::as_interrupt(std::function<void(void)> fnc, bool rising, uint32_t pri)
{
    if(!valid) return false;
    if(!allocate_interrupt_pin(gpiopin)) {
        printf("ERROR: pin line %d already has an interrupt set\n", gpiopin);
        valid= false;
        return false;
    }

    interrupt_fncs[gpiopin] = fnc;

    GPIO_InitTypeDef GPIO_InitStruct{0};
    GPIO_InitStruct.Mode = rising ? GPIO_MODE_IT_RISING : GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pin = ppin;
    GPIO_InitStruct.Pull = pullup ? GPIO_PULLUP : pulldown ? GPIO_PULLDOWN : GPIO_NOPULL;

    HAL_GPIO_Init((GPIO_TypeDef *)pport, &GPIO_InitStruct);

    NVIC_SetPriority(getIRQ(ppin), pri);
    NVIC_EnableIRQ(getIRQ(ppin));

    interrupt = true;
    return true;
}

// used to setup sd detect pin but can be used by c to setup interrupt pin
extern "C" int allocate_hal_interrupt_pin(uint16_t pin, void (*fnc)(void))
{
    uint16_t pi= gpiopin2pin(pin);
    if(pi < 16) {
        if(!Pin::allocate_interrupt_pin(pi)) {
            printf("ERROR: allocate_hal_interrupt_pin - pin line %d already has an interrupt set\n", pi);
            return 0;
        }

        interrupt_fncs[pi] = fnc;
        NVIC_SetPriority(getIRQ(pin), 15);
        NVIC_EnableIRQ(getIRQ(pin));
        return 1;
    }

    printf("ERROR: allocate_hal_interrupt_pin - illegal pin number\n");
    return 0;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint16_t pi= gpiopin2pin(GPIO_Pin);
    if(pi < 16) {
        std::function<void(void)>& fnc = interrupt_fncs[pi];
        if(fnc) {
            fnc();
        }
    }
}

extern "C" void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}
extern "C" void EXTI1_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}
extern "C" void EXTI2_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}
extern "C" void EXTI3_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}
extern "C" void EXTI4_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}
extern "C" void EXTI9_5_IRQHandler(void)
{
    // select which pin caused the interrupt
    if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_5) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_6) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_7) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_8) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_9) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
}
extern "C" void EXTI15_10_IRQHandler(void)
{
    // select which pin caused the interrupt
    if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_10) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_11) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_12) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_13) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_14) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14);
    else if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_15) != RESET)
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
}

