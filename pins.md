Note the Name is not a chip designation, it is a mapping of the
pins used on the headers when selecting a function in the Firmware

| Name       | pin     | comment                  |
| ---------- | ------- | ------------------------ |
| ADC1_0     | PA0_C   | T1 for thermistors only  |
| ADC1_1     | PF11    | T4 board                 |
| ADC1_2     | PF12    | T2                       |
| ADC1_3     | PB0     | T3                       |
| ADC1_4     | PC2     | G                        |
| ADC1_5     | PC3     | G                        |
| ADC1_6     | PA3     | G                        |
|            |         |                          |
| ADC3_0     | PC0     | N/C     |
| ADC3_1     | PF10    | G                        |
| ADC3_2     | PF8     | VFET                     |
| ADC3_3     | PF6     | G                        |
| ADC3_4     | PF4     | G                        |
| ADC3_5     | PH5     | VMOT                     |
|            |         |                          |
| PWM1_1     | PE9     |                          |
| PWM1_2     | PE11    |                          |
| PWM1_3     | PE13    |                          |
| PWM1_4     | PE14    |                          |
|            |         |                          |
| PWM2_1     | PI5     |                          |
| PWM2_2     | PI6     |                          |
| PWM2_3     | PI7     |                          |
| PWM2_4     | PI2     |                          |
|            |         |                          |
| UART 0 TX  | PD8     | H/W USART3               |
| UART 0 RX  | PB11    |                          |
| UART 1 TX  | PB9     | H/W USART4               |
| UART 1 RX  | PH14    | PB8 on Nucleo            |
|            |         |                          |
| SPI0CK     | PA5     |                          |
| SPI0MOSI   | PB5     |                          |
| SPI0MISO   | PA6     |                          |
|            |         |                          |
| SPI1CK     | PE12    | MOTOR drivers            |
| SPI1MOSI   | PE6     |                          |
| SPI1MISO   | PE5     |                          |
|            |         |                          |
| I2C0 SCL   | PF1     | PB10/11 on devebox       |
| I2C0 SDA   | PF0     | PB10/11 on devebox       |
|            |         |                          |
| I2C1 SCL   | PA8     | n/a on devebox           |
| I2C1 SDA   | PH8     |                          |


# USB
| Name       | pin     | comment                  |
| ---------- | ------- | ------------------------ |
| USB2vbus   | PA9     | (not used Prime)         |
| USB2id     | PA10    | ACM/CDC Device           |
| USB2       | PA11    | ACM/CDC Device           |
| USB2       | PA12    | ACM/CDC Device           |
|            |         |                          |
| USB1id     | PB12    | Host Device (not used)   |
| USB1vbus   | PB13    | Host Device              |
| USB1dm     | PB14    | Host Device              |
| USB1dp     | PB15    | Host Device              |

# SDCard

| Name       | pin     | comment |
| ---------- | ------- | ------- |
| SDMMCDET   | PG10    |         |
| SDMMC      | PC8     |         |
| SDMMC      | PC9     |         |
| SDMMC      | PC10    |         |
| SDMMC      | PC11    |         |
| SDMMC      | PC12    |         |
| SDMMC      | PD2     |         |


# Ethernet

| Name       | Prime   | Nucleo  |
| ---------- | ------- | ------- |
| RX_CLK     | PA1     |         |
| MDIO       | PA2     |         |
| RX_DV      | PA7     |         |
| TX_EN      | PG11    |         |
| TXD0       | PG13    |         |
| MDC        | PC1     |         |
| RXD0       | PC4     |         |
| RXD1       | PC5     |         |
| TXD1       | PG12    | PB13    |


# Quad SPI

| Name       | pin     | comment |
| ---------- | ------- | ------- |
| IO0        | PD11    |         |
| IO1        | PF9     | PD12    |
| IO2        | PE2     |         |
| IO3        | PD13    |         |
| NCS        | PB6     |         |
| CLK        | PB2     |         |

# not 5v tolerant

| pin     | comment   |
| ------- | --------- |
| PE8     |           |
| PE7     |           |
| PA4     |           |
| PB1     |           |
| PG1     |           |
| PA5     | SPI0CK    |
| PE9     | PWM1_1    |


# Nucleo Board

| Name       | pin     | comment      |
| ---------- | ------- | --------     |
| LED1       | PB0     |              |
| LED2       | PE1     |              |
| LED3       | PB14    |              |
| BUT        | PC13    |              |
| UART3_RX   | PD9     | Debug nucleo |
| UART3_TX   | PD8     | Debug nucleo |
| UART 1 RX  | PB8     |              |
| UART 1 TX  | PB9     |              |

# Prime Board builtins

| function         | pin   | comment       |
| ----------       | ----- | -------       |
| MSC LED          | PF13  |               |
| BOOT0_DETECT/MSC | PJ14  |               |
| LED1             | PH9   |               |
| LED2             | PH10  |               |
| LED3             | PH11  |               |
| LED4             | PH12  |               |
| UART2_RX         | PD6   | debug port    |
| UART2_TX         | PD5   | debug port    |
| FET NOT Enable   | PF14  |               |
| FET PowerShutoff | PD7   |               |
| Motor NOT Enable | PH13  |               |
| Motors reset     | n/a   | normally high |
| SSR1             | PI4   |               |
| SSR2             | PB7   |               |
| Hotend1          | PE0   |               |
| Hotend2          | PB8   |               |
| Bed              | PE3   |               |
| Fan 1            | PE1   |               |
| Fan 2            | PI11  |               |
| Endstop Xmin     | PD0   |               |
| Endstop Xmax     | PI3   |               |
| Endstop Ymin     | PI1   |               |
| Endstop Ymax     | PA9   |               |
| Endstop Zmin     | PI0   |               |
| Endstop Zmax     | PH15  |               |
| Probe            | PB10  |               |
| board id #3      | PE10  |               |
| board id #2      | PF3   |               |
| board id #1      | PF5   |               |
| board id #0      | PF7   |               |

# Gadgeteer headers

## GA

| hdr  | pin     | func            |
| ---- | ------- | --------------- |
| 1    |         | 3v3             |
| 2    |         | 5v              |
| 3    | PJ6     |                 |
| 4    | PJ9     |                 |
| 5    | PJ10    |                 |
| 6    | PJ11    |                 |
| 7    | PE5     | mot spi MISO    |
| 8    | PE6     | mot spi MOSI    |
| 9    | PE12    | mot spi CLK     |
| 10   |         | gnd             |

## GB

| hdr  | pin     | func      |
| ---- | ------- | --------- |
| 1    |         | 3v3       |
| 2    |         | 5v        |
| 3    | PD9     |           |
| 4    | PB9     | UART1 TX  |
| 5    | PH14    | UART1 RX  |
| 6    | PK0     |           |
| 7    | PK1     |           |
| 8    | PH8     | I2C3_SDA  |
| 9    | PA8     | I2C3_SCL  |
| 10   |         | gnd       |

## GC

| hdr  | pin     | func      |
| ---- | ------- | --------- |
| 1    |         | 3v3       |
| 2    |         | 5v        |
| 3    | PD14    |           |
| 4    | PD12    |           |
| 5    | PD10    |           |
| 6    | PI5     | PWM2_1    |
| 7    | PI6     | PWM2_2    |
| 8    | PI7     | PWM2_3    |
| 9    | PI2     | PWM2_4    |
| 10   |         | gnd       |

## GD

| hdr  | pin     | func      |
| ---- | ------- | --------- |
| 1    |         | 3v3       |
| 2    |         | 5v        |
| 3    | PE7     |           |
| 4    | PD8     | UART0 TX  |
| 5    | PB11    | UART0 RX  |
| 6    | PE9     | PWM1_1    |
| 7    | PE11    | PWM1_2    |
| 8    | PE13    | PWM1_3    |
| 9    | PE14    | PWM1_4    |
| 10   |         | gnd       |

## GE

| hdr  | pin     | func      |
| ---- | ------- | --------- |
| 1    |         | 3v3       |
| 2    |         | 5v        |
| 3    | PE8     |           |
| 4    | PJ7     |           |
| 5    | PJ8     |           |
| 6    | PD15    |           |
| 7    | PB5     | SPI1_MOSI |
| 8    | PA6     | SPI1_MISO |
| 9    | PA5     | SPI1_SCK  |
| 10   |         | gnd       |

## GF

| hdr  | pin     | func      |
| ---- | ------- | --------- |
| 1    |         | 3v3       |
| 2    |         | 5v        |
| 3    | PB1     |           |
| 4    | PJ4     |           |
| 5    | PA4     |           |
| 6    | PG0     |           |
| 7    | PJ3     |           |
| 8    | PG1     |           |
| 9    | PJ2     |           |
| 10   |         | gnd       |

## GG

| hdr  | pin     | func      |
| ---- | ------- | --------- |
| 1    |         | 3v3       |
| 2    |         | 5v        |
| 3    | PH2     |           |
| 4    | PH4     |           |
| 5    | PC0     |           |
| 6    | PF15    |           |
| 7    | PH3     |           |
| 8    | PF0     | I2C2_SDA  |
| 9    | PF1     | I2C2_SCL  |
| 10   |         | gnd       |

## GH

| hdr  | pin     | func      |
| ---- | ------- | --------- |
| 1    |         | 3v3       |
| 2    |         | 5v        |
| 3    | PC3     | ADC1_5    |
| 4    | PA3     | ADC1_6    |
| 5    | PC2     | ADC1_4    |
| 6    | PF2     |           |
| 7    | PI10    |           |
| 8    | PH7     |           |
| 9    | PB12    |           |
| 10   |         | gnd       |

## GI

| hdr  | pin     | func      |
| ---- | ------- | --------- |
| 1    |         | 3v3       |
| 2    |         | 5v        |
| 3    | PF10    | ADC3_1    |
| 4    | PF6     | ADC3_3    |
| 5    | PF4     | ADC3_4    |
| 6    | PI8     |           |
| 7    | PE4     |           |
| 8    | PI9     |           |
| 9    | PC13    |           |
| 10   |         | gnd       |
