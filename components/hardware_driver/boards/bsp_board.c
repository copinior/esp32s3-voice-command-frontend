#include "bsp_board.h"
#include "esp_custom_board.h"

#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include <stdlib.h>

#include "es7210.h"
#include "es8311.h"

static const char *TAG = "BSP_BOARD";

static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;

static es7210_dev_handle_t s_es7210 = NULL;
static es8311_handle_t s_es8311 = NULL;

static bool s_i2c_inited = false;
static bool s_board_inited = false;

/* PCA9557 registers */
#define PCA9557_REG_INPUT_PORT      0x00
#define PCA9557_REG_OUTPUT_PORT     0x01
#define PCA9557_REG_POLARITY_INV    0x02
#define PCA9557_REG_CONFIG_PORT     0x03

/* PCA9557 GPIO bits */
#define PCA9557_BIT_LCD_CS          BIT(0)
#define PCA9557_BIT_PA_EN           BIT(1)
#define PCA9557_BIT_DVP_PWDN        BIT(2)

static i2s_data_bit_width_t to_i2s_bit_width(int bits_per_chan)
{
    switch (bits_per_chan) {
    case 16: return I2S_DATA_BIT_WIDTH_16BIT;
    case 24: return I2S_DATA_BIT_WIDTH_24BIT;
    case 32: return I2S_DATA_BIT_WIDTH_32BIT;
    default: return I2S_DATA_BIT_WIDTH_16BIT;
    }
}

static es7210_i2s_bits_t to_es7210_bits(int bits_per_chan)
{
    switch (bits_per_chan) {
    case 16: return ES7210_I2S_BITS_16B;
    case 18: return ES7210_I2S_BITS_18B;
    case 20: return ES7210_I2S_BITS_20B;
    case 24: return ES7210_I2S_BITS_24B;
    case 32: return ES7210_I2S_BITS_32B;
    default: return ES7210_I2S_BITS_16B;
    }
}

static es8311_resolution_t to_es8311_res(int bits_per_chan)
{
    switch (bits_per_chan) {
    case 16: return ES8311_RESOLUTION_16;
    case 18: return ES8311_RESOLUTION_18;
    case 20: return ES8311_RESOLUTION_20;
    case 24: return ES8311_RESOLUTION_24;
    case 32: return ES8311_RESOLUTION_32;
    default: return ES8311_RESOLUTION_16;
    }
}

static esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_inited) {
        return ESP_OK;
    }

    const i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_MASTER_NUM, &conf), TAG, "i2c param config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0), TAG, "i2c install failed");

    s_i2c_inited = true;
    ESP_LOGI(TAG, "I2C initialized: port=%d sda=%d scl=%d", I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    return ESP_OK;
}

static esp_err_t pca9557_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = { reg_addr, data };
    return i2c_master_write_to_device(I2C_MASTER_NUM, PCA9557_I2C_ADDR, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
}

static esp_err_t pca9557_register_read_byte(uint8_t reg_addr, uint8_t *data)
{
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "null data");
    return i2c_master_write_read_device(I2C_MASTER_NUM, PCA9557_I2C_ADDR, &reg_addr, 1, data, 1, pdMS_TO_TICKS(1000));
}

static esp_err_t pca9557_set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t out = 0;
    ESP_RETURN_ON_ERROR(pca9557_register_read_byte(PCA9557_REG_OUTPUT_PORT, &out), TAG, "read pca9557 output failed");
    if (level) {
        out |= gpio_bit;
    } else {
        out &= ~gpio_bit;
    }
    return pca9557_register_write_byte(PCA9557_REG_OUTPUT_PORT, out);
}

static esp_err_t pa_en(uint8_t level)
{
    return pca9557_set_output_state(PCA9557_BIT_PA_EN, level);
}

static esp_err_t bsp_pca9557_init(void)
{
    /* Align reference init sequence: DVP_PWDN=1, PA_EN=0, LCD_CS=1 */
    ESP_RETURN_ON_ERROR(pca9557_register_write_byte(PCA9557_REG_OUTPUT_PORT, 0x05), TAG, "set pca9557 output failed");
    /* IO0/1/2 output, IO3..7 input => 0xF8 */
    ESP_RETURN_ON_ERROR(pca9557_register_write_byte(PCA9557_REG_CONFIG_PORT, 0xF8), TAG, "set pca9557 dir failed");
    uint8_t out = 0;
    ESP_RETURN_ON_ERROR(pca9557_register_read_byte(PCA9557_REG_OUTPUT_PORT, &out), TAG, "read pca9557 output failed");
    ESP_LOGI(TAG, "PCA9557 initialized, output=0x%02X", out);
    return ESP_OK;
}

static esp_err_t bsp_i2s_init(uint32_t sample_rate, int channel_format, int bits_per_chan)
{
    (void)channel_format;

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };

    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle), TAG, "i2s new channel failed");

    const i2s_data_bit_width_t bit_width = to_i2s_bit_width(bits_per_chan);

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = bit_width,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = bit_width,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = ES7210_MCLK_GPIO,
            .bclk = ES7210_BCLK_GPIO,
            .ws   = ES7210_LRCK_GPIO,
            .dout = ES8311_SDIN_GPIO,
            .din  = ES7210_SDOUT_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx_handle, &std_cfg), TAG, "i2s tx init failed");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(rx_handle, &std_cfg), TAG, "i2s rx init failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx_handle), TAG, "i2s tx enable failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(rx_handle), TAG, "i2s rx enable failed");

    ESP_LOGI(TAG, "I2S initialized: mclk=%d bclk=%d ws=%d dout=%d din=%d sr=%lu bits=%d",
             ES7210_MCLK_GPIO, ES7210_BCLK_GPIO, ES7210_LRCK_GPIO,
             ES8311_SDIN_GPIO, ES7210_SDOUT_GPIO, (unsigned long)sample_rate, bits_per_chan);
    return ESP_OK;
}

static esp_err_t es7210_codec_init(uint32_t sample_rate, int bits_per_chan)
{
    if (s_es7210 == NULL) {
        es7210_i2c_config_t i2c_conf = {
            .i2c_port = I2C_MASTER_NUM,
            .i2c_addr = ES7210_I2C_ADDR,
        };
        ESP_RETURN_ON_ERROR(es7210_new_codec(&i2c_conf, &s_es7210), TAG, "es7210 create failed");
    }

    es7210_codec_config_t codec_conf = {
        .i2s_format = ES7210_I2S_FMT_I2S,
        .mclk_ratio = I2S_MCLK_MULTPLE,
        .sample_rate_hz = sample_rate,
        .bit_width = to_es7210_bits(bits_per_chan),
        .mic_bias = ES7210_MIC_BIAS_2V87,
        .mic_gain = ES7210_MIC_GAIN_24DB,
        .flags.tdm_enable = 0,
    };

    ESP_RETURN_ON_ERROR(es7210_config_codec(s_es7210, &codec_conf), TAG, "es7210 codec config failed");
    ESP_RETURN_ON_ERROR(es7210_config_volume(s_es7210, 0), TAG, "es7210 volume config failed");

    ESP_LOGI(TAG, "ES7210 initialized, i2c_addr=0x%02X", ES7210_I2C_ADDR);
    return ESP_OK;
}

static esp_err_t es8311_codec_init(uint32_t sample_rate, int bits_per_chan)
{
    if (s_es8311 == NULL) {
        s_es8311 = es8311_create(I2C_MASTER_NUM, ES8311_ADDRRES_0);
        ESP_RETURN_ON_FALSE(s_es8311, ESP_FAIL, TAG, "es8311 create failed");
    }

    const es8311_clock_config_t clk_cfg = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = (int)(sample_rate * I2S_MCLK_MULTPLE),
        .sample_frequency = (int)sample_rate,
    };

    const es8311_resolution_t res = to_es8311_res(bits_per_chan);

    ESP_RETURN_ON_ERROR(es8311_init(s_es8311, &clk_cfg, res, res), TAG, "es8311 init failed");
    ESP_RETURN_ON_ERROR(es8311_sample_frequency_config(s_es8311, (int)(sample_rate * I2S_MCLK_MULTPLE), (int)sample_rate),
                        TAG, "es8311 sample freq config failed");
    ESP_RETURN_ON_ERROR(es8311_voice_volume_set(s_es8311, 70, NULL), TAG, "es8311 volume set failed");
    ESP_RETURN_ON_ERROR(es8311_microphone_config(s_es8311, false), TAG, "es8311 mic config failed");

    ESP_LOGI(TAG, "ES8311 initialized, i2c_addr=0x%02X", ES8311_I2C_ADDR);
    return ESP_OK;
}

esp_err_t bsp_board_init(uint32_t sample_rate, int channel_format, int bits_per_chan)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "i2c init failed");
    ESP_RETURN_ON_ERROR(bsp_pca9557_init(), TAG, "pca9557 init failed");

    /* 先起 I2S，再配 codec（让 MCLK/BCLK/WS 已经稳定） */
    ESP_RETURN_ON_ERROR(bsp_i2s_init(sample_rate, channel_format, bits_per_chan), TAG, "i2s init failed");

    ESP_RETURN_ON_ERROR(es7210_codec_init(sample_rate, bits_per_chan), TAG, "es7210 init failed");
    ESP_RETURN_ON_ERROR(es8311_codec_init(sample_rate, bits_per_chan), TAG, "es8311 init failed");

    /* 最后开功放，避免初始化爆音 */
    ESP_RETURN_ON_ERROR(pa_en(1), TAG, "PA enable failed");

    ESP_LOGI(TAG, "Board initialized successfully");

    s_board_inited = true;
    return ESP_OK;
}

esp_err_t bsp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len)
{
    (void)is_get_raw_channel;
    ESP_RETURN_ON_FALSE(buffer != NULL && buffer_len > 0, ESP_ERR_INVALID_ARG, TAG, "invalid feed buffer");
    ESP_RETURN_ON_FALSE(rx_handle != NULL, ESP_ERR_INVALID_STATE, TAG, "rx handle not initialized");

    static int16_t *s_stereo_rx_buf = NULL;
    static size_t s_stereo_rx_buf_bytes = 0;

    size_t mono_bytes = (size_t)buffer_len;
    size_t stereo_bytes = mono_bytes * 2;
    if (s_stereo_rx_buf == NULL || s_stereo_rx_buf_bytes < stereo_bytes) {
        int16_t *new_buf = realloc(s_stereo_rx_buf, stereo_bytes);
        ESP_RETURN_ON_FALSE(new_buf != NULL, ESP_ERR_NO_MEM, TAG, "alloc stereo rx buf failed");
        s_stereo_rx_buf = new_buf;
        s_stereo_rx_buf_bytes = stereo_bytes;
    }

    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(rx_handle, s_stereo_rx_buf, stereo_bytes, &bytes_read, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "i2s read failed");

    size_t frame_bytes = sizeof(int16_t) * 2;
    size_t frame_count = bytes_read / frame_bytes;
    size_t mono_samples = mono_bytes / sizeof(int16_t);
    if (frame_count > mono_samples) {
        frame_count = mono_samples;
    }

    uint64_t energy_l = 0;
    uint64_t energy_r = 0;
    for (size_t i = 0; i < frame_count; ++i) {
        int16_t l = s_stereo_rx_buf[i * 2];
        int16_t r = s_stereo_rx_buf[i * 2 + 1];
        energy_l += (uint64_t)(l >= 0 ? l : -l);
        energy_r += (uint64_t)(r >= 0 ? r : -r);
    }
    size_t pick = (energy_r > energy_l) ? 1 : 0;

    for (size_t i = 0; i < frame_count; ++i) {
        buffer[i] = s_stereo_rx_buf[i * 2 + pick];
    }
    for (size_t i = frame_count; i < mono_samples; ++i) {
        buffer[i] = 0;
    }

    return ESP_OK;
}

int bsp_get_feed_channel(void)
{
    return 1;
}

char *bsp_get_input_format(void)
{
    return "M";
}

esp_err_t bsp_audio_play(const int16_t *data, int length, TickType_t ticks_to_wait)
{
    if (tx_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t bytes_written = 0;
    return i2s_channel_write(tx_handle, data, length, &bytes_written, ticks_to_wait);
}

esp_err_t bsp_audio_set_play_vol(int volume)
{
    ESP_RETURN_ON_FALSE(s_es8311 != NULL, ESP_ERR_INVALID_STATE, TAG, "es8311 not initialized");
    return es8311_voice_volume_set(s_es8311, volume, NULL);
}

esp_err_t bsp_audio_get_play_vol(int *volume)
{
    ESP_RETURN_ON_FALSE(s_es8311 != NULL, ESP_ERR_INVALID_STATE, TAG, "es8311 not initialized");
    return es8311_voice_volume_get(s_es8311, volume);
}

esp_err_t bsp_sdcard_init(char *mount_point, size_t max_file)
{
    (void)mount_point;
    (void)max_file;
    return ESP_OK;
}

esp_err_t bsp_lcd_cs_set(bool level) {
    if (!s_board_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return pca9557_set_output_state(PCA9557_BIT_LCD_CS, level);
}
