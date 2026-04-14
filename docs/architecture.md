# Architecture

## 目标

这个仓库承担“语音前端”职责：

- 在 ESP32-S3 本地完成唤醒与命令词识别
- 用 LVGL 展示状态、命令与链路健康度
- 通过 ESP-NOW 将命令发给桥接节点或执行端

它不负责最终的电机闭环控制。执行端更适合放在独立的 STM32 仓库中。

## 运行链路

```text
MIC / Codec
    -> hardware_driver
    -> AFE / WakeNet / MultiNet
    -> command_dispatch_task
    -> lvgl_ui
    -> espnow_bridge
    -> ESP8266 bridge / STM32 backend
```

## 模块职责

### `main/`

- 负责应用入口 `app_main()`
- 创建 AFE、WakeNet、MultiNet 识别流程
- 建立命令队列与音频执行队列
- 做本地命令置信度过滤
- 转发本地识别结果到 ESP-NOW
- 接收远端 ESP-NOW 命令并统一调度

### `components/hardware_driver/`

- 封装板级 BSP
- 提供音频输入、音频播放、显示初始化与背光接口
- 隔离底层 I2C、I2S、LCD、Codec 初始化细节

### `components/lvgl_ui/`

- 提供语音监控页面
- 展示当前状态、命令来源、置信度、RSSI、通道号
- 展示 ESP-NOW 连续失败次数

### `components/espnow_bridge/`

- 初始化 Wi-Fi STA + ESP-NOW
- 封装发送、接收、回调注册
- 自动维护发送序列号
- 支持默认目标 MAC 和显式目标 MAC

### `tools/esp8266_esnow_slave_bridge/`

- 作为从机接收 ESP-NOW 语音命令帧
- 做去重、节流、阈值校验
- 将有效命令转换为 TTL 串口帧发给下游 MCU

## 任务模型

`main/main.c` 当前主流程包含四个核心任务：

- `feed_Task`
  - 从板级音频驱动拉取数据并喂给 AFE
- `detect_Task`
  - 执行唤醒词和命令词检测
- `command_dispatch_task`
  - 对本地/远端命令做统一过滤与分发
- `audio_exec_task`
  - 播放提示音并向 UI 报告执行状态

这种分层比较适合公开展示，因为数据流清晰，面试时也容易讲明白。

## 公开展示时建议强调的边界

- ESP32 仓库只讲“识别、显示、下发”
- STM32 仓库只讲“串口解析、状态机、FOC 执行”
- 协议字段与 `command_id` 映射单独文档化，避免逻辑散在代码里
