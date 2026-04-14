#ifndef __ESP_CUSTOM_BOARD_H
#define __ESP_CUSTOM_BOARD_H

#include <stdbool.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_common.h"

#define I2C_MASTER_NUM              I2C_NUM_1
#define I2C_MASTER_SCL_IO           GPIO_NUM_2
#define I2C_MASTER_SDA_IO           GPIO_NUM_1
#define I2C_MASTER_FREQ_HZ          100000

#define ES7210_I2C_ADDR             0x41
#define ES7210_MCLK_GPIO            GPIO_NUM_38
#define ES7210_LRCK_GPIO            GPIO_NUM_13
#define ES7210_BCLK_GPIO            GPIO_NUM_14
#define ES7210_SDOUT_GPIO           GPIO_NUM_12

#define ES8311_I2C_ADDR             0x18
#define ES8311_SDIN_GPIO            GPIO_NUM_45

#define I2S_NUM                     I2S_NUM_0
#define I2S_SAMPLE_RATE             24000
#define I2S_MCLK_MULTPLE            256

#define AUDIO_CODEC_DMA_DESC_NUM    6
#define AUDIO_CODEC_DMA_FRAME_NUM   240

#define AUDIO_CODEC_USE_PCA9557     
#define PCA9557_I2C_ADDR            0x19

/* LCD */
#define BSP_LCD_PIXEL_CLOCK_HZ          (40 * 1000 * 1000)
#define BSP_LCD_SPI_NUM                 (SPI3_HOST)
#define LCD_CMD_BITS                    (8)
#define LCD_PARAM_BITS                  (8)
#define BSP_LCD_BITS_PER_PIXEL          (16)
#define LCD_LEDC_CH                     LEDC_CHANNEL_0

#define BSP_LCD_H_RES                   (320)
#define BSP_LCD_V_RES                   (240)

#define BSP_LCD_SPI_MOSI                (GPIO_NUM_40)
#define BSP_LCD_SPI_CLK                 (GPIO_NUM_41)
#define BSP_LCD_SPI_CS                  (GPIO_NUM_NC)
#define BSP_LCD_DC                      (GPIO_NUM_39)
#define BSP_LCD_RST                     (GPIO_NUM_NC)
#define BSP_LCD_BACKLIGHT               (GPIO_NUM_42)

#define BSP_LCD_DRAM_BUF_HEIGHT         (20)

#define BSP_LCD_SPI_MODE                (2)
#define BSP_LCD_TRANS_QUEUE_DEPTH       (1)
#define BSP_LCD_SWAP_XY                 (true)
#define BSP_LCD_MIRROR_X                (true)
#define BSP_LCD_MIRROR_Y                (false)
#define BSP_LCD_OFFSET_X                (0)
#define BSP_LCD_OFFSET_Y                (0)
#define BSP_LCD_COLOR_INVERT            (true)
#define BSP_LCD_BACKLIGHT_INVERT        (true)

#endif
