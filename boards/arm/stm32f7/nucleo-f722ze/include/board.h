/****************************************************************************
 * boards/arm/stm32f7/nucleo-f722ze/include/board.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __BOARDS_ARM_STM32F7_NUCLEO_F722ZE_INCLUDE_BOARD_H
#define __BOARDS_ARM_STM32F7_NUCLEO_F722ZE_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifndef __ASSEMBLY__
#  include <stdint.h>
#endif

/* Do not include STM32 F7 header files here */

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Clocking *****************************************************************/

/* The Nucleo-f722ze  board provides the following clock sources:
 *
 *   MCO: 8 MHz from MCO output of ST-LINK is used as input clock
 *   X2:  32.768 KHz crystal for LSE
 *   X3:  HSE crystal oscillator (not provided)
 *
 * So we have these clock source available within the STM32
 *
 *   HSI: 16 MHz RC factory-trimmed
 *   LSI: 32 KHz RC
 *   HSE: 8 MHz from MCO output of ST-LINK
 *   LSE: 32.768 kHz
 */

#define STM32_BOARD_XTAL        8000000ul

#define STM32_HSI_FREQUENCY     16000000ul
#define STM32_LSI_FREQUENCY     32000
#define STM32_HSE_FREQUENCY     STM32_BOARD_XTAL
#define STM32_LSE_FREQUENCY     32768

/* Main PLL Configuration.
 *
 * PLL source is HSE = 8,000,000
 *
 * PLL_VCO = (STM32_HSE_FREQUENCY / PLLM) * PLLN
 * Subject to:
 *
 *     2 <= PLLM <= 63
 *   192 <= PLLN <= 432
 *   192 MHz <= PLL_VCO <= 432MHz
 *
 * SYSCLK  = PLL_VCO / PLLP
 * Subject to
 *
 *   PLLP = {2, 4, 6, 8}
 *   SYSCLK <= 216 MHz
 *
 * USB OTG FS, SDMMC and RNG Clock = PLL_VCO / PLLQ
 * Subject to
 *   The USB OTG FS requires a 48 MHz clock to work correctly. The SDMMC
 *   and the random number generator need a frequency lower than or equal
 *   to 48 MHz to work correctly.
 *
 * 2 <= PLLQ <= 15
 */

/* Highest SYSCLK with USB OTG FS clock = 48 MHz
 *
 * PLL_VCO = (8,000,000 / 4) * 216 = 432 MHz
 * SYSCLK  = 432 MHz / 2 = 216 MHz
 * USB OTG FS, SDMMC and RNG Clock = 432 MHz / 9 = 48 MHz
 */

#define STM32_PLLCFG_PLLM       RCC_PLLCFG_PLLM(4)
#define STM32_PLLCFG_PLLN       RCC_PLLCFG_PLLN(216)
#define STM32_PLLCFG_PLLP       RCC_PLLCFG_PLLP_2
#define STM32_PLLCFG_PLLQ       RCC_PLLCFG_PLLQ(9)

#define STM32_VCO_FREQUENCY     ((STM32_HSE_FREQUENCY / 4) * 216)
#define STM32_SYSCLK_FREQUENCY  (STM32_VCO_FREQUENCY / 2)
#define STM32_OTGFS_FREQUENCY   (STM32_VCO_FREQUENCY / 9)

/* Configure factors for  PLLSAI clock */

#define CONFIG_STM32F7_PLLSAI 1
#define STM32_RCC_PLLSAICFGR_PLLSAIN    RCC_PLLSAICFGR_PLLSAIN(192)
#define STM32_RCC_PLLSAICFGR_PLLSAIP    RCC_PLLSAICFGR_PLLSAIP(8)
#define STM32_RCC_PLLSAICFGR_PLLSAIQ    RCC_PLLSAICFGR_PLLSAIQ(4)
#define STM32_RCC_PLLSAICFGR_PLLSAIR    RCC_PLLSAICFGR_PLLSAIR(2)

/* Configure Dedicated Clock Configuration Register */

#define STM32_RCC_DCKCFGR1_PLLI2SDIVQ  RCC_DCKCFGR1_PLLI2SDIVQ(1)
#define STM32_RCC_DCKCFGR1_PLLSAIDIVQ  RCC_DCKCFGR1_PLLSAIDIVQ(1)
#define STM32_RCC_DCKCFGR1_PLLSAIDIVR  RCC_DCKCFGR1_PLLSAIDIVR(0)
#define STM32_RCC_DCKCFGR1_SAI1SRC     RCC_DCKCFGR1_SAI1SEL(0)
#define STM32_RCC_DCKCFGR1_SAI2SRC     RCC_DCKCFGR1_SAI2SEL(0)
#define STM32_RCC_DCKCFGR1_TIMPRESRC   0
#define STM32_RCC_DCKCFGR1_DFSDM1SRC   0
#define STM32_RCC_DCKCFGR1_ADFSDM1SRC  0

/* Configure factors for  PLLI2S clock */

#define CONFIG_STM32F7_PLLI2S 1
#define STM32_RCC_PLLI2SCFGR_PLLI2SN   RCC_PLLI2SCFGR_PLLI2SN(192)
#define STM32_RCC_PLLI2SCFGR_PLLI2SP   RCC_PLLI2SCFGR_PLLI2SP(2)
#define STM32_RCC_PLLI2SCFGR_PLLI2SQ   RCC_PLLI2SCFGR_PLLI2SQ(2)
#define STM32_RCC_PLLI2SCFGR_PLLI2SR   RCC_PLLI2SCFGR_PLLI2SR(2)

/* Configure Dedicated Clock Configuration Register 2 */

#define STM32_RCC_DCKCFGR2_USART1SRC  RCC_DCKCFGR2_USART1SEL_APB
#define STM32_RCC_DCKCFGR2_USART2SRC  RCC_DCKCFGR2_USART2SEL_APB
#define STM32_RCC_DCKCFGR2_UART4SRC   RCC_DCKCFGR2_UART4SEL_APB
#define STM32_RCC_DCKCFGR2_UART5SRC   RCC_DCKCFGR2_UART5SEL_APB
#define STM32_RCC_DCKCFGR2_USART6SRC  RCC_DCKCFGR2_USART6SEL_APB
#define STM32_RCC_DCKCFGR2_UART7SRC   RCC_DCKCFGR2_UART7SEL_APB
#define STM32_RCC_DCKCFGR2_UART8SRC   RCC_DCKCFGR2_UART8SEL_APB
#define STM32_RCC_DCKCFGR2_I2C1SRC    RCC_DCKCFGR2_I2C1SEL_HSI
#define STM32_RCC_DCKCFGR2_I2C2SRC    RCC_DCKCFGR2_I2C2SEL_HSI
#define STM32_RCC_DCKCFGR2_I2C3SRC    RCC_DCKCFGR2_I2C3SEL_HSI
#define STM32_RCC_DCKCFGR2_I2C4SRC    RCC_DCKCFGR2_I2C4SEL_HSI
#define STM32_RCC_DCKCFGR2_LPTIM1SRC  RCC_DCKCFGR2_LPTIM1SEL_APB
#define STM32_RCC_DCKCFGR2_CECSRC     RCC_DCKCFGR2_CECSEL_HSI
#define STM32_RCC_DCKCFGR2_CK48MSRC   RCC_DCKCFGR2_CK48MSEL_PLL
#define STM32_RCC_DCKCFGR2_SDMMCSRC   RCC_DCKCFGR2_SDMMCSEL_48MHZ
#define STM32_RCC_DCKCFGR2_SDMMC2SRC  RCC_DCKCFGR2_SDMMC2SEL_48MHZ
#define STM32_RCC_DCKCFGR2_DSISRC     RCC_DCKCFGR2_DSISEL_PHY

/* Several prescalers allow the configuration of the two AHB buses, the
 * high-speed APB (APB2) and the low-speed APB (APB1) domains. The maximum
 * frequency of the two AHB buses is 216 MHz while the maximum frequency of
 * the high-speed APB domains is 108 MHz. The maximum allowed frequency of
 * the low-speed APB domain is 54 MHz.
 */

/* AHB clock (HCLK) is SYSCLK (216 MHz) */

#define STM32_RCC_CFGR_HPRE     RCC_CFGR_HPRE_SYSCLK  /* HCLK  = SYSCLK / 1 */
#define STM32_HCLK_FREQUENCY    STM32_SYSCLK_FREQUENCY

/* APB1 clock (PCLK1) is HCLK/4 (54 MHz) */

#define STM32_RCC_CFGR_PPRE1    RCC_CFGR_PPRE1_HCLKd4     /* PCLK1 = HCLK / 4 */
#define STM32_PCLK1_FREQUENCY   (STM32_HCLK_FREQUENCY/4)

/* Timers driven from APB1 will be twice PCLK1 */

#define STM32_APB1_TIM2_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM3_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM4_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM5_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM6_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM7_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM12_CLKIN  (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM13_CLKIN  (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM14_CLKIN  (2*STM32_PCLK1_FREQUENCY)

/* APB2 clock (PCLK2) is HCLK/2 (108MHz) */

#define STM32_RCC_CFGR_PPRE2    RCC_CFGR_PPRE2_HCLKd2     /* PCLK2 = HCLK / 2 */
#define STM32_PCLK2_FREQUENCY   (STM32_HCLK_FREQUENCY/2)

/* Timers driven from APB2 will be twice PCLK2 */

#define STM32_APB2_TIM1_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM8_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM9_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM10_CLKIN  (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM11_CLKIN  (2*STM32_PCLK2_FREQUENCY)

/* SDMMC dividers.  Note that slower clocking is required when DMA
 * is disabledin order to avoid RX overrun/TX underrun errors due
 * to delayed responses to service FIFOs in interrupt driven mode.
 * These values have not been tuned!!!
 *
 * SDMMCCLK=48MHz, SDMMC_CK=SDMMCCLK/(118+2)=400 KHz
 */

#define STM32_SDMMC_INIT_CLKDIV         (118 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)

/* DMA ON:  SDMMCCLK=48MHz, SDMMC_CK=SDMMCCLK/(1+2)=16 MHz
 * DMA OFF: SDMMCCLK=48MHz, SDMMC_CK=SDMMCCLK/(2+2)=12 MHz
 */

#ifdef CONFIG_SDIO_DMA
#  define STM32_SDMMC_MMCXFR_CLKDIV     (1 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)
#else
#  define STM32_SDMMC_MMCXFR_CLKDIV     (2 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)
#endif

/* DMA ON:  SDMMCCLK=48MHz, SDMMC_CK=SDMMCCLK/(1+2)=16 MHz
 * DMA OFF: SDMMCCLK=48MHz, SDMMC_CK=SDMMCCLK/(2+2)=12 MHz
 */

#ifdef CONFIG_SDIO_DMA
#  define STM32_SDMMC_SDXFR_CLKDIV      (1 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)
#else
#  define STM32_SDMMC_SDXFR_CLKDIV      (2 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)
#endif

/* DMA Channel/Stream Selections ********************************************/

/* Stream selections are arbitrary for now but might become important in the
 * future if we set aside more DMA channels/streams.
 *
 * SDMMC DMA is on DMA2
 *
 * SDMMC1 DMA
 *   DMAMAP_SDMMC1_1 = Channel 4, Stream 3
 *   DMAMAP_SDMMC1_2 = Channel 4, Stream 6
 *
 * SDMMC2 DMA
 *   DMAMAP_SDMMC2_1 = Channel 11, Stream 0
 *   DMAMAP_SDMMC3_2 = Channel 11, Stream 5
 */

#define DMAMAP_SDMMC1  DMAMAP_SDMMC1_1
#define DMAMAP_SDMMC2  DMAMAP_SDMMC2_1

/* FLASH wait states
 *
 *  --------- ---------- -----------
 *  VDD       MAX SYSCLK WAIT STATES
 *  --------- ---------- -----------
 *  1.7-2.1 V   180 MHz    8
 *  2.1-2.4 V   216 MHz    9
 *  2.4-2.7 V   216 MHz    8
 *  2.7-3.6 V   216 MHz    7
 *  --------- ---------- -----------
 */

#define BOARD_FLASH_WAITSTATES 7

/* LED definitions **********************************************************/

/* The Nucleo-f722ze board has numerous LEDs but only three, LD1 a Green LED,
 * LD2 a Blue LED and LD3 a Red LED, that can be controlled by software.
 * The following definitions assume the default Solder Bridges are installed.
 *
 * If CONFIG_ARCH_LEDS is not defined, then the user can control the LEDs
 * in any way.
 * The following definitions are used to access individual LEDs.
 */

/* LED index values for use with board_userled() */

#define BOARD_LED1        0
#define BOARD_LED2        1
#define BOARD_LED3        2
#define BOARD_NLEDS       3

#define BOARD_LED_GREEN   BOARD_LED1
#define BOARD_LED_BLUE    BOARD_LED2
#define BOARD_LED_RED     BOARD_LED3

/* LED bits for use with board_userled_all() */

#define BOARD_LED1_BIT    (1 << BOARD_LED1)
#define BOARD_LED2_BIT    (1 << BOARD_LED2)
#define BOARD_LED3_BIT    (1 << BOARD_LED3)

/* If CONFIG_ARCH_LEDS is defined, the usage by the board port is defined in
 * include/board.h and src/stm32_leds.c. The LEDs are used to encode
 * OS-relatedevents as follows:
 *
 *
 *   SYMBOL                     Meaning                      LED state
 *                                                        Red   Green Blue
 * ----------------------  --------------------------  ------ ------ ----
 */

#define LED_STARTED        0 /* NuttX has been started   OFF    OFF   OFF  */
#define LED_HEAPALLOCATE   1 /* Heap has been allocated  OFF    OFF   ON   */
#define LED_IRQSENABLED    2 /* Interrupts enabled       OFF    ON    OFF  */
#define LED_STACKCREATED   3 /* Idle stack created       OFF    ON    ON   */
#define LED_INIRQ          4 /* In an interrupt          N/C    N/C   GLOW */
#define LED_SIGNAL         5 /* In a signal handler      N/C    GLOW  N/C  */
#define LED_ASSERTION      6 /* An assertion failed      GLOW   N/C   GLOW */
#define LED_PANIC          7 /* The system has crashed   Blink  OFF   N/C  */
#define LED_IDLE           8 /* MCU is is sleep mode     ON     OFF   OFF  */

/* Thus if the Green LED is statically on, NuttX has successfully booted and
 * is, apparently, running normally.  If the Red LED is flashing at
 * approximately 2Hz, then a fatal error has been detected and the system
 * has halted.
 */

/* Button definitions *******************************************************/

/* The STM32F7 Discovery supports one button:  Pushbutton B1, labeled "User",
 * is connected to GPIO PI11.
 * A high value will be sensed when the button is depressed.
 */

#define BUTTON_USER        0
#define NUM_BUTTONS        1
#define BUTTON_USER_BIT    (1 << BUTTON_USER)

/* DMA channels *************************************************************/

/* ADC */

#define ADC1_DMA_CHAN DMAMAP_ADC1_1
#define ADC2_DMA_CHAN DMAMAP_ADC2_1
#define ADC3_DMA_CHAN DMAMAP_ADC3_1

/* Alternate function pin selections ****************************************/

/* ADC1 */

#define GPIO_ADC1_IN0   GPIO_ADC1_IN0_0   /* PA0 */
#define GPIO_ADC1_IN1   GPIO_ADC1_IN1_0   /* PA1 */
#define GPIO_ADC1_IN2   GPIO_ADC1_IN2_0   /* PA2 */
#define GPIO_ADC1_IN3   GPIO_ADC1_IN3_0   /* PA3 */
#define GPIO_ADC1_IN4   GPIO_ADC1_IN4_0   /* PA4 */
#define GPIO_ADC1_IN5   GPIO_ADC1_IN5_0   /* PA5 */
#define GPIO_ADC1_IN6   GPIO_ADC1_IN6_0   /* PA6 */
#define GPIO_ADC1_IN7   GPIO_ADC1_IN7_0   /* PA7 */
#define GPIO_ADC1_IN8   GPIO_ADC1_IN8_0   /* PB0 */
#define GPIO_ADC1_IN9   GPIO_ADC1_IN9_0   /* PB1 */
#define GPIO_ADC1_IN10  GPIO_ADC1_IN10_0  /* PC0 */
#define GPIO_ADC1_IN11  GPIO_ADC1_IN11_0  /* PC1 */
#define GPIO_ADC1_IN12  GPIO_ADC1_IN12_0  /* PC2 */
#define GPIO_ADC1_IN13  GPIO_ADC1_IN13_0  /* PC3 */
#define GPIO_ADC1_IN14  GPIO_ADC1_IN14_0  /* PC4 */
#define GPIO_ADC1_IN15  GPIO_ADC1_IN15_0  /* PC5 */

/* TIM */

/* Quadrature encoder
 * Default is to use timer 8 (16-bit) and encoder on PC6/PC7
 * We use here TIM2 with a 32-bit counter on PA15/PB3
 */

#define GPIO_TIM1_CH1IN (GPIO_TIM1_CH1IN_2|GPIO_SPEED_50MHz)
#define GPIO_TIM1_CH2IN (GPIO_TIM1_CH2IN_2|GPIO_SPEED_50MHz)

#define GPIO_TIM2_CH1IN (GPIO_TIM2_CH1IN_2|GPIO_SPEED_50MHz)
#define GPIO_TIM2_CH2IN (GPIO_TIM2_CH2IN_2|GPIO_SPEED_50MHz)

#define GPIO_TIM3_CH1IN (GPIO_TIM3_CH1IN_2|GPIO_SPEED_50MHz)
#define GPIO_TIM3_CH2IN (GPIO_TIM3_CH2IN_2|GPIO_SPEED_50MHz)

#define GPIO_TIM4_CH1IN (GPIO_TIM4_CH1IN_2|GPIO_SPEED_50MHz)
#define GPIO_TIM4_CH2IN (GPIO_TIM4_CH2IN_2|GPIO_SPEED_50MHz)

#define GPIO_TIM8_CH1IN (GPIO_TIM8_CH1IN_1|GPIO_SPEED_50MHz)
#define GPIO_TIM8_CH2IN (GPIO_TIM8_CH2IN_1|GPIO_SPEED_50MHz)

/* PWM
 * Use Timer 1 or 3
 */

#define GPIO_TIM1_CH1OUT  (GPIO_TIM1_CH1OUT_2|GPIO_SPEED_50MHz)
#define GPIO_TIM1_CH1NOUT (GPIO_TIM1_CH1NOUT_3|GPIO_SPEED_50MHz)
#define GPIO_TIM1_CH2OUT  (GPIO_TIM1_CH2OUT_2|GPIO_SPEED_50MHz)
#define GPIO_TIM1_CH2NOUT (GPIO_TIM1_CH2NOUT_3|GPIO_SPEED_50MHz)

#define GPIO_TIM3_CH1OUT (GPIO_TIM3_CH1OUT_2|GPIO_SPEED_50MHz)
#define GPIO_TIM3_CH2OUT (GPIO_TIM3_CH2OUT_2|GPIO_SPEED_50MHz)
#define GPIO_TIM3_CH3OUT (GPIO_TIM3_CH3OUT_2|GPIO_SPEED_50MHz)
#define GPIO_TIM3_CH4OUT (GPIO_TIM3_CH4OUT_1|GPIO_SPEED_50MHz)

#if defined(CONFIG_NUCLEO_F722ZE_CONSOLE_ARDUINO)

/* USART6:
 *
 * These configurations assume that you are using a standard Arduio RS-232
 * shield with the serial interface with RX on pin D0 and TX on pin D1:
 *
 *   -------- ---------------
 *               STM32F7
 *   ARDUIONO FUNCTION  GPIO
 *   -- ----- --------- -----
 *   DO RX    USART6_RX PG9
 *   D1 TX    USART6_TX PG14
 *   -- ----- --------- -----
 */

#  define GPIO_USART6_RX (GPIO_USART6_RX_2|GPIO_SPEED_100MHz)
#  define GPIO_USART6_TX (GPIO_USART6_TX_2|GPIO_SPEED_100MHz)
#endif

/* USART3:
 * Use  USART3 and the USB virtual COM port
 */

#if defined(CONFIG_NUCLEO_F722ZE_CONSOLE_VIRTUAL)
#  define GPIO_USART3_RX (GPIO_USART3_RX_3|GPIO_SPEED_100MHz)
#  define GPIO_USART3_TX (GPIO_USART3_TX_3|GPIO_SPEED_100MHz)
#endif

#if defined(CONFIG_NUCLEO_F722ZE_CONSOLE_MORPHO_UART4)

/* UART4:
 *
 * This configuration assumes that you disabled Ethernet MII clocking
 * by removing SB13 to free PA1.
 *
 *   -------- ---------------
 *               STM32F7
 *   Pin      FUNCTION  GPIO
 *   -------  --------- -----
 *   CN11 30  UART4_RX  PA1
 *   CN11 28  UART4_TX  PA0
 *   -------  --------- -----
 */

#  define GPIO_UART4_RX (GPIO_UART4_RX_1|GPIO_SPEED_100MHz)
#  define GPIO_UART4_TX (GPIO_UART4_TX_1|GPIO_SPEED_100MHz)

/* USART3 seems to be forced selected by the Nucleo-F746ZG kconfig - bug */

#  define GPIO_USART3_RX (GPIO_USART3_RX_1|GPIO_SPEED_100MHz)
#  define GPIO_USART3_TX (GPIO_USART3_TX_1|GPIO_SPEED_100MHz)

/* USART6 seems to be forced selected by the Nucleo-F722E kconfig - bug */

#  define GPIO_USART6_RX (GPIO_USART6_RX_2|GPIO_SPEED_100MHz)
#  define GPIO_USART6_TX (GPIO_USART6_TX_2|GPIO_SPEED_100MHz)

#endif

/* USART8:
 *
 * This configurations assume that you are connecting to the Morpho connector
 * with the serial interface with the adaptor's RX on pin CN11 pin 64 and
 * TX on pin CN11 pin 61
 *
 * USART8: has no remap
 */

/* SPI
 *
 *
 *  PA6   SPI1_MISO CN12-13
 *  PA7   SPI1_MOSI CN12-15
 *  PA5   SPI1_SCK  CN12-11
 *
 *  PB14  SPI2_MISO CN12-28
 *  PB15  SPI2_MOSI CN12-26
 *  PB13  SPI2_SCK  CN12-30
 *
 *  PB4   SPI3_MISO CN12-27
 *  PB5   SPI3_MOSI CN12-29
 *  PB3   SPI3_SCK  CN12-31
 */

#define GPIO_SPI1_MISO   (GPIO_SPI1_MISO_1|GPIO_SPEED_50MHz)
#define GPIO_SPI1_MOSI   (GPIO_SPI1_MOSI_1|GPIO_SPEED_50MHz)
#define GPIO_SPI1_SCK    (GPIO_SPI1_SCK_1|GPIO_SPEED_50MHz)

#define GPIO_SPI2_MISO   (GPIO_SPI2_MISO_1|GPIO_SPEED_50MHz)
#define GPIO_SPI2_MOSI   (GPIO_SPI2_MOSI_1|GPIO_SPEED_50MHz)
#define GPIO_SPI2_SCK    (GPIO_SPI2_SCK_3|GPIO_SPEED_50MHz)

#define GPIO_SPI3_MISO   (GPIO_SPI3_MISO_1|GPIO_SPEED_50MHz)
#define GPIO_SPI3_MOSI   (GPIO_SPI3_MOSI_2|GPIO_SPEED_50MHz)
#define GPIO_SPI3_SCK    (GPIO_SPI3_SCK_1|GPIO_SPEED_50MHz)

/* I2C
 *
 *
 *  PB8   I2C1_SCL CN12-3
 *  PB9   I2C1_SDA CN12-5

 *  PB10   I2C2_SCL CN11-51
 *  PB11 I2C2_SDA CN12-18
 *
 *  PA8   I2C3_SCL CN12-23
 *  PC9   I2C3_SDA CN12-1
 *
 */

#define GPIO_I2C1_SCL (GPIO_I2C1_SCL_2|GPIO_SPEED_50MHz)
#define GPIO_I2C1_SDA (GPIO_I2C1_SDA_2|GPIO_SPEED_50MHz)

#define GPIO_I2C2_SCL (GPIO_I2C2_SCL_1|GPIO_SPEED_50MHz)
#define GPIO_I2C2_SDA (GPIO_I2C2_SDA_1|GPIO_SPEED_50MHz)

#define GPIO_I2C3_SCL (GPIO_I2C3_SCL_1|GPIO_SPEED_50MHz)
#define GPIO_I2C3_SDA (GPIO_I2C3_SDA_1|GPIO_SPEED_50MHz)

/* The STM32 F7 connects to a SMSC LAN8742A PHY using these pins:
 *
 *   STM32 F7 BOARD        LAN8742A
 *   GPIO     SIGNAL       PIN NAME
 *   -------- ------------ -------------
 *   PG11     RMII_TX_EN   TXEN
 *   PG13     RMII_TXD0    TXD0
 *   PB13     RMII_TXD1    TXD1
 *   PC4      RMII_RXD0    RXD0/MODE0
 *   PC5      RMII_RXD1    RXD1/MODE1
 *   PG2      RMII_RXER    RXER/PHYAD0 -- Not used
 *   PA7      RMII_CRS_DV  CRS_DV/MODE2
 *   PC1      RMII_MDC     MDC
 *   PA2      RMII_MDIO    MDIO
 *   N/A      NRST         nRST
 *   PA1      RMII_REF_CLK nINT/REFCLK0
 *   N/A      OSC_25M      XTAL1/CLKIN
 *
 * The PHY address is either 0 or 1, depending on the state of PG2 on reset.
 * PG2 is not controlled but appears to result in a PHY address of 0.
 */

#define GPIO_ETH_MDC          (GPIO_ETH_MDC_0|GPIO_SPEED_100MHz)
#define GPIO_ETH_MDIO         (GPIO_ETH_MDIO_0|GPIO_SPEED_100MHz)
#define GPIO_ETH_RMII_CRS_DV  (GPIO_ETH_RMII_CRS_DV_0|GPIO_SPEED_100MHz)
#define GPIO_ETH_RMII_REF_CLK (GPIO_ETH_RMII_REF_CLK_0|GPIO_SPEED_100MHz)
#define GPIO_ETH_RMII_RXD0    (GPIO_ETH_RMII_RXD0_0|GPIO_SPEED_100MHz)
#define GPIO_ETH_RMII_RXD1    (GPIO_ETH_RMII_RXD1_0|GPIO_SPEED_100MHz)
#define GPIO_ETH_RMII_TX_EN   (GPIO_ETH_RMII_TX_EN_2|GPIO_SPEED_100MHz)
#define GPIO_ETH_RMII_TXD0    (GPIO_ETH_RMII_TXD0_2|GPIO_SPEED_100MHz)
#define GPIO_ETH_RMII_TXD1    (GPIO_ETH_RMII_TXD1_1|GPIO_SPEED_100MHz)

/* CAN Bus  */

#ifdef CONFIG_NUCLEO_F722ZE_CAN1_MAP_PD0PD1
#  define GPIO_CAN1_TX  (GPIO_CAN1_TX_3|GPIO_SPEED_50MHz) /* PD1 */
#  define GPIO_CAN1_RX  (GPIO_CAN1_RX_3|GPIO_SPEED_50MHz) /* PD0 */
#elif CONFIG_NUCLEO_F722ZE_CAN1_MAP_D14D15
#  define GPIO_CAN1_TX  (GPIO_CAN1_TX_2|GPIO_SPEED_50MHz) /* PB9 */
#  define GPIO_CAN1_RX  (GPIO_CAN1_RX_2|GPIO_SPEED_50MHz) /* PB8 */
#endif

/* SDMMC2 */

#define GPIO_SDMMC2_CK  (GPIO_SDMMC2_CK_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_CMD (GPIO_SDMMC2_CMD_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_D0  (GPIO_SDMMC2_D0_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_D1  (GPIO_SDMMC2_D1_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_D2  (GPIO_SDMMC2_D2_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_D3  (GPIO_SDMMC2_D3_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_D4  (GPIO_SDMMC2_D4_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_D5  (GPIO_SDMMC2_D5_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_D6  (GPIO_SDMMC2_D6_0|GPIO_SPEED_50MHz)
#define GPIO_SDMMC2_D7  (GPIO_SDMMC2_D7_0|GPIO_SPEED_50MHz)

/* OTGFS */

#define GPIO_OTGFS_DM  (GPIO_OTGFS_DM_0|GPIO_SPEED_100MHz)
#define GPIO_OTGFS_DP  (GPIO_OTGFS_DP_0|GPIO_SPEED_100MHz)
#define GPIO_OTGFS_ID  (GPIO_OTGFS_ID_0|GPIO_SPEED_100MHz)

#endif /* __BOARDS_ARM_STM32F7_NUCLEO_F722ZE_INCLUDE_BOARD_H */
