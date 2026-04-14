# Integration

## 推荐仓库拆分

建议把整体系统拆成两个公开仓库：

1. `esp32-voice-command-frontend`
2. `stm32-foc-motor-controller`

当前仓库只保留语音前端与桥接工具，不把电机执行逻辑混进来。

## 典型系统拓扑

```text
ESP32-S3 frontend
    -> ESP-NOW
ESP8266 bridge
    -> TTL UART
STM32 motor controller
```

如果后续你希望进一步拆仓，再把 `tools/esp8266_esnow_slave_bridge/` 独立为第三个仓库即可。

## 联调顺序

### 1. 先打通 ESP32 本地识别

- 确认麦克风、Codec、显示屏初始化正常
- 确认唤醒词可进入监听状态
- 确认本地 UI 能显示 `Command`、`Phrase`、`Prob`

### 2. 再打通 ESP-NOW

- 在 ESP32 侧设置 `CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC`
- 将其改成 ESP8266 桥接板的实际 MAC
- 确认 UI 上的 `ESP-NOW` 状态不再连续失败

### 3. 最后联 STM32 执行端

- 让 ESP8266 输出 TTL 帧
- STM32 只做串口接收与协议解析
- 在 STM32 端做 `command_id -> 控制动作` 映射

## 关键集成约束

### 初始化必须先完成

建议把 `command_id = 15` 的“初始化”作为系统联调中的显式步骤。

在执行端仓库中建议明确约束：

- 未初始化前，拒绝进入位置环/速度环
- 初始化失败时，状态查询应能返回错误原因
- 重启和初始化命令应与普通运动命令分开处理

### 统一命令语义

当前命令集合既包含运动命令，也包含模式命令和系统命令：

- 运动命令：加速、减速、刹车、停车、转向、倒车
- 模式命令：位置模式、速度模式
- 系统命令：状态查询、重启系统、初始化

建议 STM32 侧把这三类命令分为不同处理分支，而不是统一落成“电机动作”。

## 公开前建议保留的目录

- `main/`
- `components/espnow_bridge/`
- `components/hardware_driver/`
- `components/lvgl_ui/`
- `tools/esp8266_esnow_slave_bridge/`
- `docs/`
- `CMakeLists.txt`
- `main/CMakeLists.txt`
- `sdkconfig.defaults*`
- `partitions*.csv`
- `dependencies.lock`

## 公开前建议不要上传的目录或文件

- `build/`
- `managed_components/`
- `sdkconfig`
- `sdkconfig.old`
- `.vscode/`
- `.claude/`

## 建议评估后再决定是否保留

- `components/perf_tester/`
- `components/player/`
- `components/sr_ringbuf/`

原因很简单：这些目录目前不是应用主链路直接依赖项。公开展示时如果保留，会让仓库看起来像“示例工程二次修改版”，而不是边界清晰的业务项目。
