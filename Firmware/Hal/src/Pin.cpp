#include "Pin.h"

#include "stm32h7xx_hal.h"

#include <algorithm>
#include <cstring>

Pin::Pin()
{
    this->inverting = false;
    this->open_drain = false;
    this->valid = false;
    this->pullup = false;
    this->pulldown = false;
}

Pin::Pin(const char *s)
{
    this->inverting = false;
    this->open_drain = false;
    this->valid = false;
    from_string(s);
}

Pin::Pin(const char *s, TYPE_T t)
{
    this->inverting = false;
    this->valid = false;
    this->open_drain = false;
    if(from_string(s) != nullptr) {
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
    //     uint16_t port = gpiocfg >> GPIO_PORT_SHIFT;
    //     uint16_t pin = gpiocfg & GPIO_PIN_MASK;
    //     set_allocated(port, pin, false);
    // }
}

// bitset to indicate a port/pin has been configured
#include <bitset>
static std::bitset<256> allocated_pins;
bool Pin::set_allocated(uint8_t port, uint8_t pin, bool set)
{
    uint8_t n = ((port-'A') * 16) + pin;

    if(!set) {
        // deallocate it
        allocated_pins.reset(n);
        return true;
    }

    if(!allocated_pins[n]) {
        // if not set yet then set it
        allocated_pins.set(n);
        return true;
    }

    // indicate it was already set
    return false;
}

extern "C" int allocate_hal_pin(void *port, uint16_t pin)
{
    uint8_t po= 0;
    if(port == GPIOA) po= 'A';
    else if(port == GPIOB) po= 'B';
    else if(port == GPIOC) po= 'C';
    else if(port == GPIOD) po= 'D';
    else if(port == GPIOE) po= 'E';
    else if(port == GPIOF) po= 'F';
    else if(port == GPIOG) po= 'G';
    else if(port == GPIOH) po= 'H';
    else if(port == GPIOI) po= 'I';
    else if(port == GPIOJ) po= 'J';
    else if(port == GPIOK) po= 'K';

    uint16_t pi= 16;
    for (int i = 0; i < 16; ++i) {
        if((pin>>i) & 1) {
            pi= i;
            break;
        }
    }

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

#define toUpper(str) (std::transform(str.begin(), str.end(), str.begin(), ::toupper), str)
// Make a new pin object from a string
// Pins are defined for the STM32xxxx as PA6 or PA_6 or PA.6, second letter is A-K followed by a number 0-15
Pin* Pin::from_string(std::string value)
{
    valid = false;
    inverting = false;
    open_drain = false;

    if(value == "nc") return nullptr;

    char port = 0;
    uint16_t pin = 0;

    if(value.size() < 3 || toupper(value[0]) != 'P') return nullptr;

    port = toupper(value[1]);
    if(port < 'A' || port > 'K') return nullptr;

    size_t pos = value.find_first_of("._", 2);
    if(pos == std::string::npos) pos= 1;
    pin= strtol(value.substr(pos + 1).c_str(), nullptr, 10);
    if(pin >= 16) return nullptr;

    if(!set_allocated(port, pin)) {
        printf("WARNING: P%c%d has already been allocated\n", port, pin);
    }

    // now check for modifiers:-
    // ! = invert pin
    // o = set pin to open drain
    // ^ = set pin to pull up
    // v = set pin to pull down
    // - = set pin to no pull up or down
    // default to pull up for input pins, neither for output
    this->pullup= true;
    this->pulldown= false;
    for(char c : value.substr(pos + 1)) {
        switch(c) {
            case '!':
                this->inverting = true;
                break;
            case 'o':
                this->open_drain= true;
                break;
            case '^':
                this->pullup= true;
                this->pulldown= false;
                break;
            case 'v':
                this->pulldown= true;
                this->pullup= false;
                break;
            case '-':
            	this->pulldown= this->pullup= false; // clear both bits
                break;
        }
    }

    // save the gpio port and pin
    gpioport = port;
    gpiopin = pin;
    ppin = 1 << pin;

    switch(port) {
    	case 'A': pport= (uint32_t *)GPIOA; __HAL_RCC_GPIOA_CLK_ENABLE(); break;
    	case 'B': pport= (uint32_t *)GPIOB; __HAL_RCC_GPIOB_CLK_ENABLE(); break;
    	case 'C': pport= (uint32_t *)GPIOC; __HAL_RCC_GPIOC_CLK_ENABLE(); break;
    	case 'D': pport= (uint32_t *)GPIOD; __HAL_RCC_GPIOD_CLK_ENABLE(); break;
    	case 'E': pport= (uint32_t *)GPIOE; __HAL_RCC_GPIOE_CLK_ENABLE(); break;
    	case 'F': pport= (uint32_t *)GPIOF; __HAL_RCC_GPIOF_CLK_ENABLE(); break;
    	case 'G': pport= (uint32_t *)GPIOG; __HAL_RCC_GPIOG_CLK_ENABLE(); break;
    	case 'H': pport= (uint32_t *)GPIOH; __HAL_RCC_GPIOH_CLK_ENABLE(); break;
    	case 'I': pport= (uint32_t *)GPIOI; __HAL_RCC_GPIOI_CLK_ENABLE(); break;
    	case 'J': pport= (uint32_t *)GPIOJ; __HAL_RCC_GPIOJ_CLK_ENABLE(); break;
    	case 'K': pport= (uint32_t *)GPIOK; __HAL_RCC_GPIOK_CLK_ENABLE(); break;
    	default: return nullptr;
    }
    this->valid = true;
    return this;
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
        }else{
        	s.append(":0");
        }
        return s;

    } else {
        return "nc";
    }
}

Pin* Pin::as_output()
{
    if(!valid) return nullptr;

    GPIO_InitTypeDef GPIO_InitStruct;
    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitTypeDef));
    GPIO_InitStruct.Mode = open_drain ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Pin = ppin;
    GPIO_InitStruct.Alternate = 0;

    pullup = pulldown= false;

    HAL_GPIO_Init((GPIO_TypeDef *)pport, &GPIO_InitStruct);
    return this;
}

Pin* Pin::as_input()
{
    if(!valid) return nullptr;

    GPIO_InitTypeDef GPIO_InitStruct;
    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitTypeDef));
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = pullup ? GPIO_PULLUP : pulldown ? GPIO_PULLDOWN : GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Pin = ppin;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init((GPIO_TypeDef *)pport, &GPIO_InitStruct);
    return this;
}


#if 0
mbed::InterruptIn* Pin::interrupt_pin()
{
    if(!this->valid) return nullptr;
    /*
        // set as input
        as_input();

        if (port_number == 0 || port_number == 2) {
            PinName pinname = port_pin((PortName)port_number, pin);
            return new mbed::InterruptIn(pinname);

        }else{
            this->valid= false;
            return nullptr;
        }
    */
    return nullptr;
}
#endif
