# Lichuang-Dev 屏幕初始化分步指南

适用目标：`ESP32-S3 + ST7789(SPI) + 立创实战派(lichuang-dev)`，并对齐你当前工程的 C 驱动写法。

## 1. 先建立硬件图谱（为什么这样连）

你这块板的显示链路是：

1. `SPI3` 负责像素/命令传输（MOSI/SCLK）。
2. `DC` 区分“命令”还是“数据”。
3. `CS` 不是直接 GPIO，而是经过 `PCA9557` IO 扩展控制。
4. `RST` 线未直接接 MCU（`GPIO_NUM_NC`），主要靠软件 init 序列。
5. 背光 `BL` 由 LEDC PWM 控制（且有反相）。

对应参考：

1. `D:\ProgrammingTools\Download\xiaozhi-esp32-main\main\boards\lichuang-dev\config.h`
2. `D:\ProgrammingTools\Download\xiaozhi-esp32-main\main\boards\lichuang-dev\lichuang_dev_board.cc`

你工程中的宏已经与参考基本一致：

1. `components/hardware_driver/boards/include/esp_custom_board.h`
2. `BSP_LCD_SPI_MOSI=40`
3. `BSP_LCD_SPI_CLK=41`
4. `BSP_LCD_DC=39`
5. `BSP_LCD_SPI_CS=GPIO_NUM_NC`（说明 CS 走扩展 IO）
6. `BSP_LCD_BACKLIGHT=42`
7. `BSP_LCD_SPI_NUM=SPI3_HOST`

## 2. 按“最小可验证单元”来写 `bsp_display_init()`

建议一次只加一步，每一步都编译+烧录验证。

### Step 1（已完成）

目标：SPI 总线初始化成功。

检查点：

1. `bsp_display_spi_init()` 只做一次。
2. `max_transfer_sz` 至少覆盖一块刷屏缓冲。

### Step 2（你来写）

目标：处理外部 CS。

写法要点：

1. 判断 `BSP_LCD_SPI_CS == GPIO_NUM_NC`。
2. 调 `bsp_lcd_cs_set(false)` 拉低 CS（参考里是 `pca9557_->SetOutputState(0, 0)`）。
3. 若失败先 `ESP_LOGW`，不要直接崩。

### Step 3（你来写）

目标：创建 panel IO。

API：`esp_lcd_new_panel_io_spi()`

关键参数：

1. `cs_gpio_num = BSP_LCD_SPI_CS`
2. `dc_gpio_num = BSP_LCD_DC`
3. `spi_mode = BSP_LCD_SPI_MODE`（lichuang-dev 为 2）
4. `pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ`（80MHz）
5. `trans_queue_depth = BSP_LCD_TRANS_QUEUE_DEPTH`
6. `lcd_cmd_bits/lcd_param_bits = 8/8`

验证标准：

1. `s_panel_io != NULL`
2. 无 `ESP_ERR_INVALID_ARG`

### Step 4（你来写）

目标：创建 ST7789 panel 驱动对象。

API：`esp_lcd_new_panel_st7789()`

关键参数：

1. `reset_gpio_num = BSP_LCD_RST`（NC）
2. `rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB`
3. `bits_per_pixel = BSP_LCD_BITS_PER_PIXEL`（16）

验证标准：

1. `s_panel != NULL`

### Step 5（你来写）

目标：真正让屏幕进入工作状态。

推荐顺序：

1. `esp_lcd_panel_reset(s_panel)`
2. `esp_lcd_panel_init(s_panel)`
3. `esp_lcd_panel_invert_color(...)`
4. `esp_lcd_panel_swap_xy(...)`
5. `esp_lcd_panel_mirror(...)`
6. `esp_lcd_panel_disp_on_off(s_panel, true)`

说明：

1. 方向参数（swap/mirror）不对就会出现画面旋转/镜像错位。
2. `disp_on_off` 对部分屏可能返回 not supported，需要兼容处理。

### Step 6（最后收尾）

目标：标记初始化完成。

1. `s_display_inited = true`
2. `return ESP_OK`

## 3. 每步失败时优先排查什么

1. Step2 失败：`bsp_board_init()` 是否先于显示初始化调用（PCA9557 还没 ready 会失败）。
2. Step3 失败：SPI host/pin 宏是否与板卡一致。
3. Step4 失败：确认组件依赖里有 `esp_lcd`，且目标芯片/IDF 版本支持 ST7789 驱动。
4. Step5 黑屏：优先查 `CS/DC`、`invert`、`swap/mirror`，其次降 `pclk_hz` 到 `40MHz` 验证信号裕量。

## 4. 建议你的实操节奏

1. 先只实现到 Step2，确认没有 I2C/PCA9557 报错。
2. 再实现 Step3+Step4，确认对象创建成功。
3. 最后实现 Step5+Step6，并用 `bsp_display_fill_color(0xF800/0x07E0/0x001F)` 做纯色测试。

按这个节奏走，定位问题会比一次性写完快很多。
