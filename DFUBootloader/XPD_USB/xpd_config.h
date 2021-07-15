/**
  ******************************************************************************
  * @file    xpd_config.h
  * @author  Benedek Kupper
  * @version 0.1
  * @date    2018-11-03
  * @brief   STM32 eXtensible Peripheral Drivers Porting to HAL
  *
  * Copyright (c) 2018 Benedek Kupper
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */
#ifndef __XPD_CONFIG_H_
#define __XPD_CONFIG_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* TODO Include the proper CMSIS device header */
#include "stm32xxxx.h"

#define HSE_VALUE_Hz               HSE_VALUE

/* Replace type definitions */
#define USB_TypeDef                __USB_TypeDef
//#define USB_OTG_GlobalTypeDef      __USB_OTG_GlobalTypeDef
#define USB_OTG_INEndpointTypeDef  __USB_OTG_INEndpointTypeDef
#define USB_OTG_OUTEndpointTypeDef __USB_OTG_OUTEndpointTypeDef
#define USB_OTG_HostChannelTypeDef __USB_OTG_HostChannelTypeDef
//#define USB_OTG_DeviceTypeDef      __USB_OTG_DeviceTypeDef
//#define USB_OTG_HostTypeDef        __USB_OTG_HostTypeDef
#define USB_OTG_TypeDef            __USB_OTG_TypeDef
#define USB_HS_PHYC_GlobalTypeDef  __USB_HS_PHYC_GlobalTypeDef

#ifdef USB
#undef USB
#define USB ((__USB_TypeDef*)USB_BASE)
#endif

#ifdef USB_HS_PHYC
#undef USB_HS_PHYC
#define USB_HS_PHYC ((__USB_HS_PHYC_GlobalTypeDef*)USB_HS_PHYC_CONTROLLER_BASE)
#endif

/* Correct bit namings */
#ifdef USB_LPMCSR_LMPEN
#define USB_LPMCSR_LPMEN           USB_LPMCSR_LMPEN
#endif

/* TODO Copy USB peripheral registers structures from a compatible device header */

/* typedef of USB_TypeDef */
typedef struct {
    union {
        struct {
            __IO uint16_t EA : 4;                           /*!<  EndPoint Address */
            __IO uint16_t STAT_TX : 2;                      /*!<  EndPoint TX Status */
            __IO uint16_t DTOG_TX : 1;                      /*!<  EndPoint Data TOGGLE TX */
            __IO uint16_t CTR_TX : 1;                       /*!<  EndPoint Correct TRansfer TX */
            __IO uint16_t KIND : 1;                         /*!<  EndPoint KIND */
            __IO uint16_t TYPE : 2;                         /*!<  EndPoint TYPE */
            __IO uint16_t SETUP : 1;                        /*!<  EndPoint SETUP */
            __IO uint16_t STAT_RX : 2;                      /*!<  EndPoint RX Status */
            __IO uint16_t DTOG_RX : 1;                      /*!<  EndPoint Data TOGGLE RX */
            __IO uint16_t CTR_RX : 1;                       /*!<  EndPoint Correct TRansfer RX */
        } b;
        __IO uint16_t w;
             uint32_t __RESERVED;
    } EPR[8];                                               /*!< USB endpoint register */
         uint32_t __RESERVED0[8];
    union {
        struct {
            __IO uint16_t FRES : 1;                         /*!< Force USB RESet */
            __IO uint16_t PDWN : 1;                         /*!< Power DoWN */
            __IO uint16_t LPMODE : 1;                       /*!< Low-power MODE */
            __IO uint16_t FSUSP : 1;                        /*!< Force SUSPend */
            __IO uint16_t RESUME : 1;                       /*!< RESUME request */
            __IO uint16_t L1RESUME : 1;                     /*!< LPM L1 Resume request */
                 uint16_t __RESERVED0 : 1;
            __IO uint16_t L1REQM : 1;                       /*!< LPM L1 state request interrupt mask */
            __IO uint16_t ESOFM : 1;                        /*!< Expected Start Of Frame Mask */
            __IO uint16_t SOFM : 1;                         /*!< Start Of Frame Mask */
            __IO uint16_t RESETM : 1;                       /*!< RESET Mask   */
            __IO uint16_t SUSPM : 1;                        /*!< SUSPend Mask */
            __IO uint16_t WKUPM : 1;                        /*!< WaKe UP Mask */
            __IO uint16_t ERRM : 1;                         /*!< ERRor Mask */
            __IO uint16_t PMAOVRM : 1;                      /*!< DMA OVeR/underrun Mask */
            __IO uint16_t CTRM : 1;                         /*!< Correct TRansfer Mask */
        } b;
        __IO uint16_t w;
    } CNTR;                                                 /*!< Control register,                       Address offset: 0x40 */
         uint16_t __RESERVED1;
    union {
        struct {
            __IO uint16_t EP_ID : 4;                        /*!< EndPoint IDentifier (read-only bit)  */
            __IO uint16_t DIR : 1;                          /*!< DIRection of transaction (read-only bit)  */
                 uint16_t __RESERVED0 : 2;
            __IO uint16_t L1REQ : 1;                        /*!< LPM L1 state request  */
            __IO uint16_t ESOF : 1;                         /*!< Expected Start Of Frame (clear-only bit) */
            __IO uint16_t SOF : 1;                          /*!< Start Of Frame (clear-only bit) */
            __IO uint16_t RESET : 1;                        /*!< RESET (clear-only bit) */
            __IO uint16_t SUSP : 1;                         /*!< SUSPend (clear-only bit) */
            __IO uint16_t WKUP : 1;                         /*!< WaKe UP (clear-only bit) */
            __IO uint16_t ERR : 1;                          /*!< ERRor (clear-only bit) */
            __IO uint16_t PMAOVR : 1;                       /*!< DMA OVeR/underrun (clear-only bit) */
            __IO uint16_t CTR : 1;                          /*!< Correct TRansfer (clear-only bit) */
        } b;
        __IO uint16_t w;
    } ISTR;                                                 /*!< Interrupt status register,              Address offset: 0x44 */
         uint16_t __RESERVED2;
    union {
        struct {
            __IO uint16_t FN : 11;                          /*!< Frame Number */
            __IO uint16_t LSOF : 2;                         /*!< Lost SOF */
            __IO uint16_t LCK : 1;                          /*!< LoCKed */
            __IO uint16_t RXDM : 1;                         /*!< status of D- data line */
            __IO uint16_t RXDP : 1;                         /*!< status of D+ data line */
        } b;
        __IO uint16_t w;
    } FNR;                                                  /*!< Frame number register,                  Address offset: 0x48 */
         uint16_t __RESERVED3;
    union {
        struct {
            __IO uint16_t ADD : 7;                          /*!< USB device address */
            __IO uint16_t EF : 1;                           /*!< USB device address Enable Function */
                 uint16_t __RESERVED0 : 8;
        } b;
        __IO uint16_t w;
    } DADDR;                                                /*!< Device address register,                Address offset: 0x4C */
         uint16_t __RESERVED4;
    __IO uint16_t BTABLE;                                   /*!< Buffer Table address register,          Address offset: 0x50 */
         uint16_t __RESERVED5;
    union {
        struct {
            __IO uint16_t LPMEN : 1;                        /*!< LPM support enable  */
            __IO uint16_t LPMACK : 1;                       /*!< LPM Token acknowledge enable*/
                 uint16_t __RESERVED0 : 1;
            __IO uint16_t REMWAKE : 1;                      /*!< bRemoteWake value received with last ACKed LPM Token */
            __IO uint16_t BESL : 4;                         /*!< BESL value received with last ACKed LPM Token  */
                 uint16_t __RESERVED1 : 8;
        } b;
        __IO uint16_t w;
    } LPMCSR;                                               /*!< LPM Control and Status register,        Address offset: 0x54 */
         uint16_t __RESERVED6;
    union {
        struct {
            __IO uint16_t BCDEN : 1;                        /*!< Battery charging detector (BCD) enable */
            __IO uint16_t DCDEN : 1;                        /*!< Data contact detection (DCD) mode enable */
            __IO uint16_t PDEN : 1;                         /*!< Primary detection (PD) mode enable */
            __IO uint16_t SDEN : 1;                         /*!< Secondary detection (SD) mode enable */
            __IO uint16_t DCDET : 1;                        /*!< Data contact detection (DCD) status */
            __IO uint16_t PDET : 1;                         /*!< Primary detection (PD) status */
            __IO uint16_t SDET : 1;                         /*!< Secondary detection (SD) status */
            __IO uint16_t PS2DET : 1;                       /*!< PS2 port or proprietary charger detected */
                 uint16_t __RESERVED0 : 7;
            __IO uint16_t DPPU : 1;                         /*!< DP Pull-up Enable */
        } b;
        __IO uint16_t w;
    } BCDR;                                                 /*!< Battery Charging detector register,     Address offset: 0x58 */
} USB_TypeDef;

/* AND/OR */
/* typedef of USB_OTG_INEndpointTypeDef
   typedef of USB_OTG_OUTEndpointTypeDef
   typedef of USB_OTG_HostChannelTypeDef
   typedef of USB_OTG_TypeDef */

/**
  * @brief USB_OTG_IN_Endpoint-Specific_Register
  */
typedef struct {
    union {
        struct {
            __IO uint32_t MPSIZ : 11;                       /*!< Maximum packet size */
                 uint32_t __RESERVED0 : 4;
            __IO uint32_t USBAEP : 1;                       /*!< USB active endpoint */
            __IO uint32_t EONUM_DPID : 1;                   /*!< Even/odd frame */
            __IO uint32_t NAKSTS : 1;                       /*!< NAK status */
            __IO uint32_t EPTYP : 2;                        /*!< Endpoint type */
                 uint32_t __RESERVED1 : 1;
            __IO uint32_t STALL : 1;                        /*!< STALL handshake */
            __IO uint32_t TXFNUM : 4;                       /*!< TxFIFO number */
            __IO uint32_t CNAK : 1;                         /*!< Clear NAK */
            __IO uint32_t SNAK : 1;                         /*!< Set NAK */
            __IO uint32_t SD0PID_SEVNFRM : 1;               /*!< Set DATA0 PID */
            __IO uint32_t SODDFRM : 1;                      /*!< Set odd frame */
            __IO uint32_t EPDIS : 1;                        /*!< Endpoint disable */
            __IO uint32_t EPENA : 1;                        /*!< Endpoint enable */
        } b;
        __IO uint32_t w;
    } DIEPCTL;                                              /* dev IN Endpoint Control Reg 900h + (ep_num * 20h) + 00h*/
         uint32_t __RESERVED0;
    union {
        struct {
            __IO uint32_t XFRC : 1;                         /*!< Transfer completed interrupt */
            __IO uint32_t EPDISD : 1;                       /*!< Endpoint disabled interrupt */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t TOC : 1;                          /*!< Timeout condition */
            __IO uint32_t ITTXFE : 1;                       /*!< IN token received when TxFIFO is empty */
                 uint32_t __RESERVED1 : 1;
            __IO uint32_t INEPNE : 1;                       /*!< IN endpoint NAK effective */
            __IO uint32_t TXFE : 1;                         /*!< Transmit FIFO empty */
            __IO uint32_t TXFIFOUDRN : 1;                   /*!< Transmit Fifo Underrun */
            __IO uint32_t BNA : 1;                          /*!< Buffer not available interrupt */
                 uint32_t __RESERVED2 : 1;
            __IO uint32_t PKTDRPSTS : 1;                    /*!< Packet dropped status */
            __IO uint32_t BERR : 1;                         /*!< Babble error interrupt */
            __IO uint32_t NAK : 1;                          /*!< NAK interrupt */
                 uint32_t __RESERVED3 : 18;
        } b;
        __IO uint32_t w;
    } DIEPINT;                                              /* dev IN Endpoint Itr Reg     900h + (ep_num * 20h) + 08h*/
         uint32_t __RESERVED1;
    union {
        struct {
            __IO uint32_t XFRSIZ : 19;                      /*!< Transfer size */
            __IO uint32_t PKTCNT : 10;                      /*!< Packet count */
            __IO uint32_t MULCNT : 2;                       /*!< Packet count */
                 uint32_t __RESERVED0 : 1;
        } b;
        __IO uint32_t w;
    } DIEPTSIZ;                                             /* IN Endpoint Txfer Size   900h + (ep_num * 20h) + 10h*/
    __IO uint32_t DIEPDMA;                                  /* IN Endpoint DMA Address Reg    900h + (ep_num * 20h) + 14h*/
    __IO uint32_t DTXFSTS;                                  /* IN Endpoint Tx FIFO Status Reg 900h + (ep_num * 20h) + 18h*/
         uint32_t __RESERVED2;
} USB_OTG_INEndpointTypeDef;


/**
  * @brief USB_OTG_OUT_Endpoint-Specific_Registers
  */
typedef struct {
    union {
        struct {
            __IO uint32_t MPSIZ : 11;                       /*!< Maximum packet size */
                 uint32_t __RESERVED0 : 4;
            __IO uint32_t USBAEP : 1;                       /*!< USB active endpoint */
                 uint32_t __RESERVED1 : 1;
            __IO uint32_t NAKSTS : 1;                       /*!< NAK status */
            __IO uint32_t EPTYP : 2;                        /*!< Endpoint type */
            __IO uint32_t SNPM : 1;                         /*!< Snoop mode */
            __IO uint32_t STALL : 1;                        /*!< STALL handshake */
                 uint32_t __RESERVED2 : 4;
            __IO uint32_t CNAK : 1;                         /*!< Clear NAK */
            __IO uint32_t SNAK : 1;                         /*!< Set NAK */
            __IO uint32_t SD0PID_SEVNFRM : 1;               /*!< Set DATA0 PID */
            __IO uint32_t SODDFRM : 1;                      /*!< Set odd frame */
            __IO uint32_t EPDIS : 1;                        /*!< Endpoint disable */
            __IO uint32_t EPENA : 1;                        /*!< Endpoint enable */
        } b;
        __IO uint32_t w;
    } DOEPCTL;                                              /* dev OUT Endpoint Control Reg  B00h + (ep_num * 20h) + 00h*/
         uint32_t __RESERVED0;
    union {
        struct {
            __IO uint32_t XFRC : 1;                         /*!< Transfer completed interrupt */
            __IO uint32_t EPDISD : 1;                       /*!< Endpoint disabled interrupt */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t STUP : 1;                         /*!< SETUP phase done */
            __IO uint32_t OTEPDIS : 1;                      /*!< OUT token received when endpoint disabled */
                 uint32_t __RESERVED1 : 1;
            __IO uint32_t B2BSTUP : 1;                      /*!< Back-to-back SETUP packets received */
                 uint32_t __RESERVED2 : 7;
            __IO uint32_t NYET : 1;                         /*!< NYET interrupt */
                 uint32_t __RESERVED3 : 17;
        } b;
        __IO uint32_t w;
    } DOEPINT;                                              /* dev OUT Endpoint Itr Reg      B00h + (ep_num * 20h) + 08h*/
         uint32_t __RESERVED1;
    union {
        struct {
            __IO uint32_t XFRSIZ : 19;                      /*!< Transfer size */
            __IO uint32_t PKTCNT : 10;                      /*!< Packet count */
            __IO uint32_t STUPCNT : 2;                      /*!< SETUP packet count */
                 uint32_t __RESERVED0 : 1;
        } b;
        __IO uint32_t w;
    } DOEPTSIZ;                                             /* dev OUT Endpoint Txfer Size   B00h + (ep_num * 20h) + 10h*/
    __IO uint32_t DOEPDMA;                                  /* dev OUT Endpoint DMA Address  B00h + (ep_num * 20h) + 14h*/
         uint32_t __RESERVED2[2];
} USB_OTG_OUTEndpointTypeDef;

/**
  * @brief USB_OTG_Host_Channel_Specific_Registers
  */
typedef struct {
    union {
        struct {
            __IO uint32_t MPSIZ : 11;                       /*!< Maximum packet size */
            __IO uint32_t EPNUM : 4;                        /*!< Endpoint number */
            __IO uint32_t EPDIR : 1;                        /*!< Endpoint direction */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t LSDEV : 1;                        /*!< Low-speed device */
            __IO uint32_t EPTYP : 2;                        /*!< Endpoint type */
            __IO uint32_t MC : 2;                           /*!< Multi Count (MC) / Error Count (EC) */
            __IO uint32_t DAD : 7;                          /*!< Device address */
            __IO uint32_t ODDFRM : 1;                       /*!< Odd frame */
            __IO uint32_t CHDIS : 1;                        /*!< Channel disable */
            __IO uint32_t CHENA : 1;                        /*!< Channel enable */
        } b;
        __IO uint32_t w;
    } HCCHAR;
    union {
        struct {
            __IO uint32_t PRTADDR : 7;                      /*!< Port address */
            __IO uint32_t HUBADDR : 7;                      /*!< Hub address */
            __IO uint32_t XACTPOS : 2;                      /*!< XACTPOS */
            __IO uint32_t COMPLSPLT : 1;                    /*!< Do complete split */
                 uint32_t __RESERVED0 : 14;
            __IO uint32_t SPLITEN : 1;                      /*!< Split enable */
        } b;
        __IO uint32_t w;
    } HCSPLT;
    union {
        struct {
            __IO uint32_t XFRC : 1;                         /*!< Transfer completed */
            __IO uint32_t CHH : 1;                          /*!< Channel halted */
            __IO uint32_t AHBERR : 1;                       /*!< AHB error */
            __IO uint32_t STALL : 1;                        /*!< STALL response received interrupt */
            __IO uint32_t NAK : 1;                          /*!< NAK response received interrupt */
            __IO uint32_t ACK : 1;                          /*!< ACK response received/transmitted interrupt */
            __IO uint32_t NYET : 1;                         /*!< Response received interrupt */
            __IO uint32_t TXERR : 1;                        /*!< Transaction error */
            __IO uint32_t BBERR : 1;                        /*!< Babble error */
            __IO uint32_t FRMOR : 1;                        /*!< Frame overrun */
            __IO uint32_t DTERR : 1;                        /*!< Data toggle error */
                 uint32_t __RESERVED0 : 21;
        } b;
        __IO uint32_t w;
    } HCINT;
    union {
        struct {
            __IO uint32_t XFRCM : 1;                        /*!< Transfer completed mask */
            __IO uint32_t CHHM : 1;                         /*!< Channel halted mask */
            __IO uint32_t AHBERR : 1;                       /*!< AHB error */
            __IO uint32_t STALLM : 1;                       /*!< STALL response received interrupt mask */
            __IO uint32_t NAKM : 1;                         /*!< NAK response received interrupt mask */
            __IO uint32_t ACKM : 1;                         /*!< ACK response received/transmitted interrupt mask */
            __IO uint32_t NYET : 1;                         /*!< response received interrupt mask */
            __IO uint32_t TXERRM : 1;                       /*!< Transaction error mask */
            __IO uint32_t BBERRM : 1;                       /*!< Babble error mask */
            __IO uint32_t FRMORM : 1;                       /*!< Frame overrun mask */
            __IO uint32_t DTERRM : 1;                       /*!< Data toggle error mask */
                 uint32_t __RESERVED0 : 21;
        } b;
        __IO uint32_t w;
    } HCINTMSK;
    union {
        struct {
            __IO uint32_t XFRSIZ : 19;                      /*!< Transfer size */
            __IO uint32_t PKTCNT : 10;                      /*!< Packet count */
            __IO uint32_t DPID : 2;                         /*!< Data PID */
            __IO uint32_t DOPING : 1;                       /*!< Do PING */
        } b;
        __IO uint32_t w;
    } HCTSIZ;
    __IO uint32_t HCDMA;
         uint32_t __RESERVED0[2];
} USB_OTG_HostChannelTypeDef;



/**
  * @brief USB_OTG
  */
typedef struct {
    union {
        struct {
            __IO uint32_t SRQSCS : 1;                       /*!< Session request success */
            __IO uint32_t SRQ : 1;                          /*!< Session request */
            __IO uint32_t VBVALOEN : 1;                     /*!< VBUS valid override enable */
            __IO uint32_t VBVALOVAL : 1;                    /*!< VBUS valid override value */
            __IO uint32_t AVALOEN : 1;                      /*!< A-peripheral session valid override enable */
            __IO uint32_t AVALOVAL : 1;                     /*!< A-peripheral session valid override value */
            __IO uint32_t BVALOEN : 1;                      /*!< B-peripheral session valid override enable */
            __IO uint32_t BVALOVAL : 1;                     /*!< B-peripheral session valid override value  */
            __IO uint32_t HNGSCS : 1;                       /*!< Host set HNP enable */
            __IO uint32_t HNPRQ : 1;                        /*!< HNP request */
            __IO uint32_t HSHNPEN : 1;                      /*!< Host set HNP enable */
            __IO uint32_t DHNPEN : 1;                       /*!< Device HNP enabled */
            __IO uint32_t EHEN : 1;                         /*!< Embedded host enable */
                 uint32_t __RESERVED0 : 3;
            __IO uint32_t CIDSTS : 1;                       /*!< Connector ID status */
            __IO uint32_t DBCT : 1;                         /*!< Long/short debounce time */
            __IO uint32_t ASVLD : 1;                        /*!< A-session valid  */
            __IO uint32_t BSVLD : 1;                        /*!< B-session valid */
            __IO uint32_t OTGVER : 1;                       /*!< OTG version  */
                 uint32_t __RESERVED1 : 11;
        } b;
        __IO uint32_t w;
    } GOTGCTL;                                              /*!< USB_OTG Control and Status Register          000h */
    union {
        struct {
                 uint32_t __RESERVED0 : 2;
            __IO uint32_t SEDET : 1;                        /*!< Session end detected                   */
                 uint32_t __RESERVED1 : 5;
            __IO uint32_t SRSSCHG : 1;                      /*!< Session request success status change  */
            __IO uint32_t HNSSCHG : 1;                      /*!< Host negotiation success status change */
                 uint32_t __RESERVED2 : 7;
            __IO uint32_t HNGDET : 1;                       /*!< Host negotiation detected              */
            __IO uint32_t ADTOCHG : 1;                      /*!< A-device timeout change                */
            __IO uint32_t DBCDNE : 1;                       /*!< Debounce done                          */
            __IO uint32_t IDCHNG : 1;                       /*!< Change in ID pin input value           */
                 uint32_t __RESERVED3 : 11;
        } b;
        __IO uint32_t w;
    } GOTGINT;                                              /*!< USB_OTG Interrupt Register                   004h */
    union {
        struct {
            __IO uint32_t GINT : 1;                         /*!< Global interrupt mask */
            __IO uint32_t HBSTLEN : 4;                      /*!< Burst length/type */
            __IO uint32_t DMAEN : 1;                        /*!< DMA enable */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t TXFELVL : 1;                      /*!< TxFIFO empty level */
            __IO uint32_t PTXFELVL : 1;                     /*!< Periodic TxFIFO empty level */
                 uint32_t __RESERVED1 : 23;
        } b;
        __IO uint32_t w;
    } GAHBCFG;                                              /*!< Core AHB Configuration Register              008h */
    union {
        struct {
            __IO uint32_t TOCAL : 3;                        /*!< FS timeout calibration */
            __IO uint32_t PHYIF : 1;                        /*!< PHY Interface (PHYIf) */
            __IO uint32_t ULPI_UTMI_SEL : 1;                /*!< ULPI or UTMI+ Select (ULPI_UTMI_Sel) */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t PHYSEL : 1;                       /*!< USB 2.0 high-speed ULPI PHY or USB 1.1 full-speed serial transceiver select */
                 uint32_t __RESERVED1 : 1;
            __IO uint32_t SRPCAP : 1;                       /*!< SRP-capable */
            __IO uint32_t HNPCAP : 1;                       /*!< HNP-capable */
            __IO uint32_t TRDT : 4;                         /*!< USB turnaround time */
                 uint32_t __RESERVED2 : 1;
            __IO uint32_t PHYLPCS : 1;                      /*!< PHY Low-power clock select */
                 uint32_t __RESERVED3 : 1;
            __IO uint32_t ULPIFSLS : 1;                     /*!< ULPI FS/LS select               */
            __IO uint32_t ULPIAR : 1;                       /*!< ULPI Auto-resume                */
            __IO uint32_t ULPICSM : 1;                      /*!< ULPI Clock SuspendM             */
            __IO uint32_t ULPIEVBUSD : 1;                   /*!< ULPI External VBUS Drive        */
            __IO uint32_t ULPIEVBUSI : 1;                   /*!< ULPI external VBUS indicator    */
            __IO uint32_t TSDPS : 1;                        /*!< TermSel DLine pulsing selection */
            __IO uint32_t PCCI : 1;                         /*!< Indicator complement            */
            __IO uint32_t PTCI : 1;                         /*!< Indicator pass through          */
            __IO uint32_t ULPIIPD : 1;                      /*!< ULPI interface protect disable  */
                 uint32_t __RESERVED4 : 3;
            __IO uint32_t FHMOD : 1;                        /*!< Forced host mode                */
            __IO uint32_t FDMOD : 1;                        /*!< Forced peripheral mode          */
            __IO uint32_t CTXPKT : 1;                       /*!< Corrupt Tx packet               */
        } b;
        __IO uint32_t w;
    } GUSBCFG;                                              /*!< Core USB Configuration Register              00Ch */
    union {
        struct {
            __IO uint32_t CSRST : 1;                        /*!< Core soft reset          */
            __IO uint32_t HSRST : 1;                        /*!< HCLK soft reset          */
            __IO uint32_t FCRST : 1;                        /*!< Host frame counter reset */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t RXFFLSH : 1;                      /*!< RxFIFO flush             */
            __IO uint32_t TXFFLSH : 1;                      /*!< TxFIFO flush             */
            __IO uint32_t TXFNUM : 5;                       /*!< TxFIFO number */
                 uint32_t __RESERVED1 : 19;
            __IO uint32_t DMAREQ : 1;                       /*!< DMA request signal */
            __IO uint32_t AHBIDL : 1;                       /*!< AHB master idle */
        } b;
        __IO uint32_t w;
    } GRSTCTL;                                              /*!< Core Reset Register                          010h */
    union {
        struct {
            __IO uint32_t CMOD : 1;                         /*!< Current mode of operation                      */
            __IO uint32_t MMIS : 1;                         /*!< Mode mismatch interrupt                        */
            __IO uint32_t OTGINT : 1;                       /*!< OTG interrupt                                  */
            __IO uint32_t SOF : 1;                          /*!< Start of frame                                 */
            __IO uint32_t RXFLVL : 1;                       /*!< RxFIFO nonempty                                */
            __IO uint32_t NPTXFE : 1;                       /*!< Nonperiodic TxFIFO empty                       */
            __IO uint32_t GINAKEFF : 1;                     /*!< Global IN nonperiodic NAK effective            */
            __IO uint32_t BOUTNAKEFF : 1;                   /*!< Global OUT NAK effective                       */
                 uint32_t __RESERVED0 : 2;
            __IO uint32_t ESUSP : 1;                        /*!< Early suspend                                  */
            __IO uint32_t USBSUSP : 1;                      /*!< USB suspend                                    */
            __IO uint32_t USBRST : 1;                       /*!< USB reset                                      */
            __IO uint32_t ENUMDNE : 1;                      /*!< Enumeration done                               */
            __IO uint32_t ISOODRP : 1;                      /*!< Isochronous OUT packet dropped interrupt       */
            __IO uint32_t EOPF : 1;                         /*!< End of periodic frame interrupt                */
                 uint32_t __RESERVED1 : 2;
            __IO uint32_t IEPINT : 1;                       /*!< IN endpoint interrupt                          */
            __IO uint32_t OEPINT : 1;                       /*!< OUT endpoint interrupt                         */
            __IO uint32_t IISOIXFR : 1;                     /*!< Incomplete isochronous IN transfer             */
            __IO uint32_t PXFR_INCOMPISOOUT : 1;            /*!< Incomplete periodic transfer                   */
            __IO uint32_t DATAFSUSP : 1;                    /*!< Data fetch suspended                           */
            __IO uint32_t RSTDET : 1;                       /*!< Reset detected interrupt                       */
            __IO uint32_t HPRTINT : 1;                      /*!< Host port interrupt                            */
            __IO uint32_t HCINT : 1;                        /*!< Host channels interrupt                        */
            __IO uint32_t PTXFE : 1;                        /*!< Periodic TxFIFO empty                          */
            __IO uint32_t LPMINT : 1;                       /*!< LPM interrupt                                  */
            __IO uint32_t CIDSCHG : 1;                      /*!< Connector ID status change                     */
            __IO uint32_t DISCINT : 1;                      /*!< Disconnect detected interrupt                  */
            __IO uint32_t SRQINT : 1;                       /*!< Session request/new session detected interrupt */
            __IO uint32_t WKUINT : 1;                       /*!< Resume/remote wakeup detected interrupt        */
        } b;
        __IO uint32_t w;
    } GINTSTS;                                              /*!< Core Interrupt Register                      014h */
    union {
        struct {
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t MMISM : 1;                        /*!< Mode mismatch interrupt mask                        */
            __IO uint32_t OTGINT : 1;                       /*!< OTG interrupt mask                                  */
            __IO uint32_t SOFM : 1;                         /*!< Start of frame mask                                 */
            __IO uint32_t RXFLVLM : 1;                      /*!< Receive FIFO nonempty mask                          */
            __IO uint32_t NPTXFEM : 1;                      /*!< Nonperiodic TxFIFO empty mask                       */
            __IO uint32_t GINAKEFFM : 1;                    /*!< Global nonperiodic IN NAK effective mask            */
            __IO uint32_t GONAKEFFM : 1;                    /*!< Global OUT NAK effective mask                       */
                 uint32_t __RESERVED1 : 2;
            __IO uint32_t ESUSPM : 1;                       /*!< Early suspend mask                                  */
            __IO uint32_t USBSUSPM : 1;                     /*!< USB suspend mask                                    */
            __IO uint32_t USBRST : 1;                       /*!< USB reset mask                                      */
            __IO uint32_t ENUMDNEM : 1;                     /*!< Enumeration done mask                               */
            __IO uint32_t ISOODRPM : 1;                     /*!< Isochronous OUT packet dropped interrupt mask       */
            __IO uint32_t EOPFM : 1;                        /*!< End of periodic frame interrupt mask                */
                 uint32_t __RESERVED2 : 1;
            __IO uint32_t EPMISM : 1;                       /*!< Endpoint mismatch interrupt mask                    */
            __IO uint32_t IEPINT : 1;                       /*!< IN endpoints interrupt mask                         */
            __IO uint32_t OEPINT : 1;                       /*!< OUT endpoints interrupt mask                        */
            __IO uint32_t IISOIXFRM : 1;                    /*!< Incomplete isochronous IN transfer mask             */
            __IO uint32_t PXFRM_IISOOXFRM : 1;              /*!< Incomplete periodic transfer mask                   */
            __IO uint32_t FSUSPM : 1;                       /*!< Data fetch suspended mask                           */
            __IO uint32_t RSTDEM : 1;                       /*!< Reset detected interrupt mask                       */
            __IO uint32_t PRTIM : 1;                        /*!< Host port interrupt mask                            */
            __IO uint32_t HCIM : 1;                         /*!< Host channels interrupt mask                        */
            __IO uint32_t PTXFEM : 1;                       /*!< Periodic TxFIFO empty mask                          */
            __IO uint32_t LPMINTM : 1;                      /*!< LPM interrupt Mask                                  */
            __IO uint32_t CIDSCHGM : 1;                     /*!< Connector ID status change mask                     */
            __IO uint32_t DISCINT : 1;                      /*!< Disconnect detected interrupt mask                  */
            __IO uint32_t SRQIM : 1;                        /*!< Session request/new session detected interrupt mask */
            __IO uint32_t WUIM : 1;                         /*!< Resume/remote wakeup detected interrupt mask        */
        } b;
        __IO uint32_t w;
    } GINTMSK;                                              /*!< Core Interrupt Mask Register                 018h */
    union {
        struct {
            __IO uint32_t CHNUM_EPNUM : 4;                  /*!< IN EP interrupt mask bits */
            __IO uint32_t BCNT : 11;                        /*!< OUT EP interrupt mask bits */
            __IO uint32_t DPID : 2;                         /*!< OUT EP interrupt mask bits */
            __IO uint32_t PKTSTS : 4;                       /*!< OUT EP interrupt mask bits */
            __IO uint32_t FRMNUM : 4;
                 uint32_t __RESERVED0 : 7;
        } b;
        __IO uint32_t w;
    } GRXSTSR;                                              /*!<  Receive Sts Q Read Register                  01Ch*/
    union {
        struct {
            __IO uint32_t CHNUM_EPNUM : 4;
            __IO uint32_t BCNT : 11;
            __IO uint32_t DPID : 2;
            __IO uint32_t PKTSTS : 4;
            __IO uint32_t FRMNUM : 4;
                 uint32_t __RESERVED0 : 7;
        } b;
        __IO uint32_t w;
    } GRXSTSP;                                              /*!<  Receive Sts Q Read & POP Register            020h*/
    __IO uint32_t GRXFSIZ;                                  /* Receive FIFO Size Register                      024h*/
    union {
        struct {
            __IO uint32_t INEPTXSA : 16;                    /*!< IN endpoint FIFOx transmit RAM start address */
            __IO uint32_t INEPTXFD : 16;                    /*!< IN endpoint TxFIFO depth */
        } b;
        __IO uint32_t w;
    } DIEPTXF0_HNPTXFSIZ;                                   /*!< EP0 / Non Periodic Tx FIFO Size Register     028h */
    __IO uint32_t HNPTXSTS;                                 /*!< Non Periodic Tx FIFO/Queue Sts reg           02Ch */
         uint32_t __RESERVED0[2];
    union {
#ifdef USB_OTG_GCCFG_VBDEN
        struct {
            __IO uint32_t DCDET : 1;                        /*!< Data contact detection (DCD) status */
            __IO uint32_t PDET : 1;                         /*!< Primary detection (PD) status */
            __IO uint32_t SDET : 1;                         /*!< Secondary detection (SD) status */
            __IO uint32_t PS2DET : 1;                       /*!< DM pull-up detection status */
                 uint32_t __RESERVED0 : 12;
            __IO uint32_t PWRDWN : 1;                       /*!< Power down */
            __IO uint32_t BCDEN : 1;                        /*!< Battery charging detector (BCD) enable */
            __IO uint32_t DCDEN : 1;                        /*!< Data contact detection (DCD) mode enable*/
            __IO uint32_t PDEN : 1;                         /*!< Primary detection (PD) mode enable*/
            __IO uint32_t SDEN : 1;                         /*!< Secondary detection (SD) mode enable */
            __IO uint32_t VBDEN : 1;                        /*!< VBUS mode enable */
            __IO uint32_t OTGIDEN : 1;                      /*!< OTG Id enable */
            __IO uint32_t PHYHSEN : 1;                      /*!< HS PHY enable */
                 uint32_t __RESERVED1 : 8;
        } b;
#else
        struct {
                 uint32_t __RESERVED0 : 16;
            __IO uint32_t PWRDWN : 1;                       /*!< Power down */
                 uint32_t __RESERVED1 : 1;
            __IO uint32_t VBUSASEN : 1;                     /*!< Enable the VBUS sensing device */
            __IO uint32_t VBUSBSEN : 1;                     /*!< Enable the VBUS sensing device */
            __IO uint32_t SOFOUTEN : 1;                     /*!< SOF output enable */
            __IO uint32_t NOVBUSSENS : 1;                   /*!< VBUS sensing disable option*/
                 uint32_t __RESERVED2 : 10;
        } b;
#endif
        __IO uint32_t w;
    } GCCFG;                                                /* General Purpose IO Register                     038h*/
    __IO uint32_t CID;                                      /* User ID Register                                03Ch*/
    __IO uint32_t GSNPSID;                                  /* USB_OTG core ID                                 040h*/
    __IO uint32_t GHWCFG1;                                  /* User HW config1                                 044h*/
    __IO uint32_t GHWCFG2;                                  /* User HW config2                                 048h*/
    union {
        struct {
                 uint32_t __RESERVED0 : 14;
            __IO uint32_t LPMMode : 1;                      /* LPM mode specified for Mode of Operation */
                 uint32_t __RESERVED1 : 17;
        } b;
        __IO uint32_t w;
    } GHWCFG3;                                              /* User HW config3                                 04Ch*/
         uint32_t __RESERVED1;
    union {
        struct {
            __IO uint32_t LPMEN : 1;                        /*!< LPM support enable                                     */
            __IO uint32_t LPMACK : 1;                       /*!< LPM Token acknowledge enable                           */
            __IO uint32_t BESL : 4;                         /*!< BESL value received with last ACKed LPM Token          */
            __IO uint32_t REMWAKE : 1;                      /*!< bRemoteWake value received with last ACKed LPM Token   */
            __IO uint32_t L1SSEN : 1;                       /*!< L1 shallow sleep enable                                */
            __IO uint32_t BESLTHRS : 4;                     /*!< BESL threshold                                         */
            __IO uint32_t L1DSEN : 1;                       /*!< L1 deep sleep enable                                   */
            __IO uint32_t LPMRSP : 2;                       /*!< LPM response                                           */
            __IO uint32_t SLPSTS : 1;                       /*!< Port sleep status                                      */
            __IO uint32_t L1RSMOK : 1;                      /*!< Sleep State Resume OK                                  */
            __IO uint32_t LPMCHIDX : 4;                     /*!< LPM Channel Index                                      */
            __IO uint32_t LPMRCNT : 3;                      /*!< LPM retry count                                        */
            __IO uint32_t SNDLPM : 1;                       /*!< Send LPM transaction                                   */
            __IO uint32_t LPMRCNTSTS : 3;                   /*!< LPM retry count status                                 */
            __IO uint32_t ENBESL : 1;                       /*!< Enable best effort service latency                     */
                 uint32_t __RESERVED0 : 3;
        } b;
        __IO uint32_t w;
    } GLPMCFG;                                              /* LPM Register                                    054h*/
    union {
        struct {
                 uint32_t __RESERVED0 : 6;
            __IO uint32_t DISABLEVBUS : 1;                  /*!< Power down */
                 uint32_t __RESERVED1 : 25;
        } b;
        __IO uint32_t w;
    } GPWRDN;                                               /* Power Down Register                             058h*/
    __IO uint32_t GDFIFOCFG;                                /* DFIFO Software Config Register                  05Ch*/
    __IO uint32_t GADPCTL;                                  /* ADP Timer, Control and Status Register          60Ch*/
         uint32_t __RESERVED2[39];
    union {
        struct {
            __IO uint32_t PTXSA : 16;                       /*!< Host periodic TxFIFO start address */
            __IO uint32_t PTXFD : 16;                       /*!< Host periodic TxFIFO depth */
        } b;
        __IO uint32_t w;
    } HPTXFSIZ;                                             /* Host Periodic Tx FIFO Size Reg                  100h*/
    union {
        struct {
            __IO uint32_t INEPTXSA : 16;                    /*!< IN endpoint FIFOx transmit RAM start address */
            __IO uint32_t INEPTXFD : 16;                    /*!< IN endpoint TxFIFO depth */
        } b;
        __IO uint32_t w;
    } DIEPTXF[5];                                           /* dev Periodic Transmit FIFO */
         uint32_t __RESERVED3[186];
    union {
        struct {
            __IO uint32_t FSLSPCS : 2;                      /*!< FS/LS PHY clock select */
            __IO uint32_t FSLSS : 1;                        /*!< FS- and LS-only support */
                 uint32_t __RESERVED0 : 29;
        } b;
        __IO uint32_t w;
    } HCFG;                                                 /* Host Configuration Register    400h*/
    __IO uint32_t HFIR;                                     /* Host Frame Interval Register   404h*/
    union {
        struct {
            __IO uint32_t FRNUM : 16;                       /*!< Frame number */
            __IO uint32_t FTREM : 16;                       /*!< Frame time remaining */
        } b;
        __IO uint32_t w;
    } HFNUM;                                                /* Host Frame Nbr/Frame Remaining 408h*/
         uint32_t __RESERVED4;
    union {
        struct {
            __IO uint32_t PTXFSAVL : 16;                    /*!< Periodic transmit data FIFO space available */
            __IO uint32_t PTXQSAV : 8;                      /*!< Periodic transmit request queue space available */
            __IO uint32_t PTXQTOP : 8;                      /*!< Top of the periodic transmit request queue */
        } b;
        __IO uint32_t w;
    } HPTXSTS;                                              /* Host Periodic Tx FIFO/ Queue Status 410h*/
    __IO uint32_t HAINT;                                    /* Host All Channels Interrupt Register 414h*/
    __IO uint32_t HAINTMSK;                                 /* Host All Channels Interrupt Mask 418h*/
         uint32_t __RESERVED5[9];
    union {
        struct {
            __IO uint32_t PCSTS : 1;                        /*!< Port connect status */
            __IO uint32_t PCDET : 1;                        /*!< Port connect detected */
            __IO uint32_t PENA : 1;                         /*!< Port enable */
            __IO uint32_t PENCHNG : 1;                      /*!< Port enable/disable change */
            __IO uint32_t POCA : 1;                         /*!< Port overcurrent active */
            __IO uint32_t POCCHNG : 1;                      /*!< Port overcurrent change */
            __IO uint32_t PRES : 1;                         /*!< Port resume */
            __IO uint32_t PSUSP : 1;                        /*!< Port suspend */
            __IO uint32_t PRST : 1;                         /*!< Port reset */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t PLSTS : 2;                        /*!< Port line status */
            __IO uint32_t PPWR : 1;                         /*!< Port power */
            __IO uint32_t PTCTL : 4;                        /*!< Port test control */
            __IO uint32_t PSPD : 2;                         /*!< Port speed */
                 uint32_t __RESERVED1 : 13;
        } b;
        __IO uint32_t w;
    } HPRT;                                                 /*!< Host Port Control and Status Register    Address offset : 0x440 */
         uint32_t __RESERVED6[47];
    USB_OTG_HostChannelTypeDef HC[12];                      /*!< Host Channels                            Address offset : 0x500-0x67F */
         uint32_t __RESERVED7[96];
    union {
        struct {
            __IO uint32_t DSPD : 2;                         /*!< Device speed */
            __IO uint32_t NZLSOHSK : 1;                     /*!< Nonzero-length status OUT handshake */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t DAD : 7;                          /*!< Device address */
            __IO uint32_t PFIVL : 2;                        /*!< Periodic (micro)frame interval */
                 uint32_t __RESERVED1 : 11;
            __IO uint32_t PERSCHIVL : 2;                    /*!< Periodic scheduling interval */
                 uint32_t __RESERVED2 : 6;
        } b;
        __IO uint32_t w;
    } DCFG;                                                 /* dev Configuration Register   800h*/
    union {
        struct {
            __IO uint32_t RWUSIG : 1;                       /*!< Remote wakeup signaling */
            __IO uint32_t SDIS : 1;                         /*!< Soft disconnect */
            __IO uint32_t GINSTS : 1;                       /*!< Global IN NAK status */
            __IO uint32_t GONSTS : 1;                       /*!< Global OUT NAK status */
            __IO uint32_t TCTL : 3;                         /*!< Test control */
            __IO uint32_t SGINAK : 1;                       /*!< Set global IN NAK */
            __IO uint32_t CGINAK : 1;                       /*!< Clear global IN NAK */
            __IO uint32_t SGONAK : 1;                       /*!< Set global OUT NAK */
            __IO uint32_t CGONAK : 1;                       /*!< Clear global OUT NAK */
            __IO uint32_t POPRGDNE : 1;                     /*!< Power-on programming done */
                 uint32_t __RESERVED0 : 20;
        } b;
        __IO uint32_t w;
    } DCTL;                                                 /* dev Control Register         804h*/
    union {
        struct {
            __IO uint32_t SUSPSTS : 1;                      /*!< Suspend status */
            __IO uint32_t ENUMSPD : 2;                      /*!< Enumerated speed */
            __IO uint32_t EERR : 1;                         /*!< Erratic error */
                 uint32_t __RESERVED0 : 4;
            __IO uint32_t FNSOF : 14;                       /*!< Frame number of the received SOF */
                 uint32_t __RESERVED1 : 10;
        } b;
        __IO uint32_t w;
    } DSTS;                                                 /* dev Status Register (RO)     808h*/
         uint32_t __RESERVED8;
    union {
        struct {
            __IO uint32_t XFRCM : 1;                        /*!< Transfer completed interrupt mask */
            __IO uint32_t EPDM : 1;                         /*!< Endpoint disabled interrupt mask */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t TOM : 1;                          /*!< Timeout condition mask (nonisochronous endpoints) */
            __IO uint32_t ITTXFEMSK : 1;                    /*!< IN token received when TxFIFO empty mask */
            __IO uint32_t INEPNMM : 1;                      /*!< IN token received with EP mismatch mask */
            __IO uint32_t INEPNEM : 1;                      /*!< IN endpoint NAK effective mask */
                 uint32_t __RESERVED1 : 1;
            __IO uint32_t TXFURM : 1;                       /*!< FIFO underrun mask */
            __IO uint32_t BIM : 1;                          /*!< BNA interrupt mask */
                 uint32_t __RESERVED2 : 22;
        } b;
        __IO uint32_t w;
    } DIEPMSK;                                              /* dev IN Endpoint Mask         810h*/
    union {
        struct {
            __IO uint32_t XFRCM : 1;                        /*!< Transfer completed interrupt mask */
            __IO uint32_t EPDM : 1;                         /*!< Endpoint disabled interrupt mask               */
            __IO uint32_t AHBERRM : 1;                      /*!< OUT transaction AHB Error interrupt mask    */
            __IO uint32_t STUPM : 1;                        /*!< SETUP phase done mask                          */
            __IO uint32_t OTEPDM : 1;                       /*!< OUT token received when endpoint disabled mask */
            __IO uint32_t OTEPSPRM : 1;                     /*!< Status Phase Received mask                     */
            __IO uint32_t B2BSTUP : 1;                      /*!< Back-to-back SETUP packets received mask       */
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t OPEM : 1;                         /*!< OUT packet error mask                          */
            __IO uint32_t BOIM : 1;                         /*!< BNA interrupt mask                             */
                 uint32_t __RESERVED1 : 2;
            __IO uint32_t BERRM : 1;                        /*!< Babble error interrupt mask                   */
            __IO uint32_t NAKM : 1;                         /*!< OUT Packet NAK interrupt mask                  */
            __IO uint32_t NYETM : 1;                        /*!< NYET interrupt mask                           */
                 uint32_t __RESERVED2 : 17;
        } b;
        __IO uint32_t w;
    } DOEPMSK;                                              /*!< dev OUT Endpoint Mask        814h */
    union {
        struct {
            __IO uint32_t IEPINT : 16;                      /*!< IN endpoint interrupt bits */
            __IO uint32_t OEPINT : 16;                      /*!< OUT endpoint interrupt bits */
        } b;
        __IO uint32_t w;
    } DAINT;                                                /* dev All Endpoints Itr Reg    818h*/
    union {
        struct {
            __IO uint32_t IEPM : 16;                        /*!< IN EP interrupt mask bits */
            __IO uint32_t OEPM : 16;                        /*!< OUT EP interrupt mask bits */
        } b;
        __IO uint32_t w;
    } DAINTMSK;                                             /* dev All Endpoints Itr Mask   81Ch*/
         uint32_t __RESERVED9[2];
    __IO uint32_t DVBUSDIS;                                 /* dev VBUS discharge Register  828h*/
    __IO uint32_t DVBUSPULSE;                               /* dev VBUS Pulse Register      82Ch*/
    union {
        struct {
            __IO uint32_t NONISOTHREN : 1;                  /*!< Nonisochronous IN endpoints threshold enable */
            __IO uint32_t ISOTHREN : 1;                     /*!< ISO IN endpoint threshold enable */
            __IO uint32_t TXTHRLEN : 9;                     /*!< Transmit threshold length */
                 uint32_t __RESERVED0 : 5;
            __IO uint32_t RXTHREN : 1;                      /*!< Receive threshold enable */
            __IO uint32_t RXTHRLEN : 9;                     /*!< Receive threshold length */
                 uint32_t __RESERVED1 : 1;
            __IO uint32_t ARPEN : 1;                        /*!< Arbiter parking enable */
                 uint32_t __RESERVED2 : 4;
        } b;
        __IO uint32_t w;
    } DTHRCTL;                                              /* dev thr                      830h*/
    __IO uint32_t DIEPEMPMSK;                               /* dev empty msk             834h*/
    union {
        struct {
                 uint32_t __RESERVED0 : 1;
            __IO uint32_t IEP1INT : 1;                      /*!< IN endpoint 1interrupt bit */
                 uint32_t __RESERVED1 : 15;
            __IO uint32_t OEP1INT : 1;                      /*!< OUT endpoint 1 interrupt bit */
                 uint32_t __RESERVED2 : 14;
        } b;
        __IO uint32_t w;
    } DEACHINT;                                             /* dedicated EP interrupt       838h*/
    __IO uint32_t DEACHMSK;                                 /* dedicated EP msk             83Ch*/
         uint32_t __RESERVED10;
    __IO uint32_t DINEP1MSK;                                /* dedicated EP mask           844h*/
         uint32_t __RESERVED11[15];
    __IO uint32_t DOUTEP1MSK;                               /* dedicated EP msk            884h*/
         uint32_t __RESERVED12[30];
    USB_OTG_INEndpointTypeDef IEP[8];
         uint32_t __RESERVED13[64];
    USB_OTG_OUTEndpointTypeDef OEP[8];
         uint32_t __RESERVED14[128];
    union {
        struct {
            __IO uint32_t STOPCLK : 1;
            __IO uint32_t GATECLK : 1;
                 uint32_t __RESERVED0 : 2;
            __IO uint32_t PHYSUSP : 1;
            __IO uint32_t ENL1GTG : 1;
            __IO uint32_t PHYSLEEP : 1;
            __IO uint32_t SUSP : 1;
                 uint32_t __RESERVED1 : 24;
        } b;
        __IO uint32_t w;
    } PCGCCTL;
         uint32_t __RESERVED15[127];
    union {
             __IO uint32_t DR;
             uint32_t __RESERVED0[1024];
    } DFIFO[8];
} USB_OTG_TypeDef;


/**
  * @brief USB_HS_PHY_Registers
  */
typedef struct {
    union {
        struct {
            __IO uint32_t PLLEN : 1;            /*!< Enable PLL */
            __IO uint32_t PLLSEL : 3;           /*!< Controls PHY frequency operation selection */
                 uint32_t __RESERVED0 : 28;
        } b;
        __IO uint32_t w;
    } PLL1;                                     /*!< This register is used to control the PLL of the HS PHY.                       000h */
         uint32_t __RESERVED0[2];
    union {
        struct {
            __IO uint32_t INCURREN : 1;
            __IO uint32_t INCURRINT : 1;
            __IO uint32_t LFSCAPEN : 1;
            __IO uint32_t HSDRVSLEW : 1;
            __IO uint32_t HSDRVDCCUR : 1;
            __IO uint32_t HSDRVDCLEV : 1;
            __IO uint32_t HSDRVCURINCR : 1;
            __IO uint32_t FSDRVRFADJ : 1;
            __IO uint32_t HSDRVRFRED : 1;
            __IO uint32_t HSDRVCHKITRM : 4;
            __IO uint32_t HSDRVCHKZTRM : 2;
            __IO uint32_t SQLCHCTL : 2;
            __IO uint32_t HDRXGNEQEN : 1;
            __IO uint32_t STAGSEL : 1;
            __IO uint32_t HSFALLPREEM : 1;
            __IO uint32_t HSRXOFF : 2;
            __IO uint32_t SHTCCTCTLPROT : 1;
            __IO uint32_t SQLBYP : 1;
                 uint32_t __RESERVED0 : 8;
        } b;
        __IO uint32_t w;
    } TUNE;                                     /*!< This register is used to control the tuning interface of the High Speed PHY.  00Ch */
         uint32_t __RESERVED1[2];
    union {
         struct {
             __IO uint32_t USED : 1;            /*!< Monitors the usage status of the PHY's LDO   */
             __IO uint32_t STATUS : 1;          /*!< Monitors the status of the PHY's LDO.        */
             __IO uint32_t DISABLE : 1;         /*!< Controls disable of the High Speed PHY's LDO */
                  uint32_t __RESERVED0 : 29;
         } b;
         __IO uint32_t w;
    } LDO;                                      /*!< This register is used to control the regulator (LDO).                         018h */
} USB_HS_PHYC_GlobalTypeDef;

#ifdef __cplusplus
}
#endif

#endif /* __XPD_CONFIG_H_ */
