Note the Name is not a chip designation, it is a mapping of the
pins used on the headers when selecting a function in the Firmware


| Name       | pin     | comment                  |
| ---------- | ------- | ------------------------ |
| ADC1_0     | PA0_C   | ADC1 are for thermistors |
| ADC1_1     | PF11    |                          |
| ADC1_2     | PF12    |                          |
| ADC1_3     | PB0     |                          |
| ADC1_4     | PC2     |                          |
| ADC1_5     | PC3     |                          |
| ADC1_6     | PA3     |                          |
|            |         |                          |
| ADC3_0     | PC0     | for voltage inputs       |
| ADC3_1     | PF10    | for voltage inputs       |
| ADC3_2     | PF8     |                          |
| ADC3_3     | PF6     |                          |
| ADC3_4     | PF4     |                          |
| ADC3_5     | PH5     |                          |
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
| UART3_RX   | PD9     | Debug port               |
| UART3_TX   | PD8     | Debug port               |
| UART2_RX   | PD6     | comms port               |
| UART2_TX   | PD5     | comms port               |
| UART4_RX   | PH14    | control port             |
| UART4_TX   | PB9     | control port             |
|            |         |                          |
| SPI0CK     | PA5     |                          |
| SPI0MOSI   | PB5     |                          |
| SPI0MISO   | PA6     |                          |
|            |         |                          |
| SPI1CK     | PE12    |                          |
| SPI1MOSI   | PE6     |                          |
| SPI1MISO   | PE5     |                          |
|            |         |                          |
| I2C0 SCL   | PF1     | PB10/11 on devebox       |
| I2C0 SDA   | PF0     | PB10/11 on devebox       |
|            |         |                          |
| I2C1 SCL   | PA8     | n/a on devebox           |
| I2C1 SDA   | PH8     |                          |
|            |         |                          |
| LED1       | PB0     | Nucleo                   |
| LED2       | PE1     | Nucleo                   |
| LED3       | PB14    | Nucleo                   |
| BUT        | PC13    | Nucleo                   |
+------------+---------+--------------------------+
| USB        |         |                          |
+------------+---------+--------------------------+
| USB2vbus   | PA9     | ACM/CDC Device           |
| USB2id     | PA10    | ACM/CDC Device           |
| USB2       | PA11    | ACM/CDC Device           |
| USB2       | PA12    | ACM/CDC Device           |
|            |         |                          |
| USB1id     | PB12    | Host Device (not used)   |
| USB1vbus   | PB13    | Host Device              |
| USB1dm     | PB14    | Host Device              |
| USB1dp     | PB15    | Host Device              |
+------------+---------+--------------------------+
| SDCard     |         |                          |
+------------+---------+--------------------------+
| SDMMCDET   | PG10    |                          |
| SDMMC      | PC8     |                          |
| SDMMC      | PC9     |                          |
| SDMMC      | PC10    |                          |
| SDMMC      | PC11    |                          |
| SDMMC      | PC12    |                          |
| SDMMC      | PD2     |                          |
+------------+---------+--------------------------+
| Ethernet   | Prime   | Nucleo                   |
+------------+---------+--------------------------+
| RX_CLK     | PA1     |                          |
| MDIO       | PA2     |                          |
| RX_DV      | PA7     |                          |
| TX_EN      | PB11    | PG11                     |
| TXD0       | PB12    | PG13                     |
| MDC        | PC1     |                          |
| RXD0       | PC4     |                          |
| RXD1       | PC5     |                          |
| TXD1       | PG14    | PB13                     |
+------------+---------+--------------------------+
| Quad SPI   | Prime   | Devebox                  |
+------------+---------+--------------------------+
| IO0        | PD11    |                          |
| IO1        | PF9     | PD12                     |
| IO2        | PE2     |                          |
| IO3        | PD13    |                          |
| NCS        | PB6     |                          |
| CLK        | PB2     |                          |
+------------+---------+--------------------------+

not 5v tolerant 
+-----+--------+
| PE8 |        |
| PE7 |        |
| PA4 |        |
| PB1 |        |
| PG1 |        |
| PA5 | SPI0CK |
| PE9 | PWM1_1 |
+-----+--------+

# Gadgeteer headers
## GA
| hdr | pin  | func         |
+-----+------+--------------+
| 1   |      | 3v3          |
| 2   |      | 5v           |
| 3   | PJ6  |              |
| 4   | PJ9  |              |
| 5   | PJ10 |              |
| 6   | PJ11 |              |
| 7   | PB5  | mot spi MOSI |
| 8   | PE6  | mot spi MISO |
| 9   | PE12 | mot spi CLK  |
| 10  |      | gnd          |
+-----+------+--------------+

## GB
| hdr | pin | func |
+-----+-----+------+
| 1   |     | 3v3  |
| 2   |     | 5v   |
| 3   |     |      |
| 4   |     |      |
| 5   |     |      |
| 6   |     |      |
| 7   |     |      |
| 8   |     |      |
| 9   |     |      |
| 10  |     | gnd  |
+-----+-----+------+
