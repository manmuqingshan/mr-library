#ifndef _MR_CONFIG_H_
#define _MR_CONFIG_H_

#include "mrboard.h"

//<---------------------------- Options -------------------------------------->
//<<< Configuration switch >>>
#define MR_CONF_DISABLE                 0
#define MR_CONF_ENABLE                  1

//<<< Debug level >>>
#define MR_CONF_DEBUG_ASSERT            0
#define MR_CONF_DEBUG_ERROR             1
#define MR_CONF_DEBUG_WARNING           2
#define MR_CONF_DEBUG_INFO              3
#define MR_CONF_DEBUG_DEBUG             4

//<---------------------------- Configuration -------------------------------------->
//<<< Object >>>
#define MR_CONF_NAME_MAX                12

//<<< Debug >>>
#define MR_CONF_DEBUG                   MR_CONF_ENABLE
#define MR_CONF_ASSERT                  MR_CONF_ENABLE
#define MR_CONF_DEBUG_LEVEL             MR_CONF_DEBUG_DEBUG

//<<< Event >>>
#define MR_CONF_EVENT                   MR_CONF_ENABLE

//<<< Device >>>
#define MR_CONF_DEVICE                  MR_CONF_ENABLE
#if (MR_CONF_DEVICE == MR_CONF_ENABLE)
//< pin >
#define MR_CONF_PIN                     MR_CONF_ENABLE
//< serial >
#define MR_CONF_SERIAL                  MR_CONF_ENABLE
#define MR_CONF_SERIAL_BUFSZ            64
//< spi >
#define MR_CONF_SPI                     MR_CONF_ENABLE
//< i2c >
#define MR_CONF_I2C                     MR_CONF_ENABLE
//< adc >
#define MR_CONF_ADC                     MR_CONF_ENABLE
//< dac >
#define MR_CONF_DAC                     MR_CONF_ENABLE
//< timer >
#define MR_CONF_TIMER                   MR_CONF_ENABLE
//< console >
#define MR_CONF_CONSOLE                 MR_CONF_ENABLE
#define MR_CONF_CONSOLE_BUFSZ           128
#define MR_CONF_CONSOLE_NAME            "uart1"
#endif

#endif