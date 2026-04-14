# ESP32 Voice Motor Control Frontend

基于 ESP32-S3 的离线语音前端项目，用于完成唤醒词检测、命令词识别、本地状态显示，以及通过 ESP-NOW 将控制命令转发给下游桥接节点或执行端。

这个仓库适合作为“语音识别前端”单独公开；如果你同时有 STM32 FOC 执行端，建议拆成两个仓库并通过协议文档互相链接。

## 项目定位

- 负责离线语音识别链路，不依赖云端
- 负责本地 UI 反馈与调试显示
- 负责把命令通过 ESP-NOW 发给 ESP8266 桥接固件或其他 ESP-NOW 节点
- 不直接实现电机闭环控制，电机执行逻辑建议放在独立的 STM32 仓库

## 当前能力

- 唤醒词 + 命令词识别
- 本地命令与远端命令统一调度
- LVGL 界面显示当前状态、命令来源、置信度与 ESP-NOW 通信状态
- ESP-NOW 命令发送、接收、心跳发送
- 附带一个 ESP8266 Arduino 桥接固件，可将 ESP-NOW 数据转成 TTL 串口帧

## 仓库结构

```text
main/                               应用入口与语音识别主流程
components/espnow_bridge/           ESP-NOW 通信封装
components/hardware_driver/         板级 BSP 与音频/显示初始化
components/lvgl_ui/                 LVGL 监控界面
tools/esp8266_esnow_slave_bridge/   ESP8266 Arduino 桥接固件
docs/                               架构、协议、联调说明
```

当前 `main/CMakeLists.txt` 只依赖以下业务组件：

- `hardware_driver`
- `espnow_bridge`
- `lvgl_ui`

这也是公开时最建议保留的核心代码边界。

## 命令词集合

当前 `sdkconfig.defaults.esp32s3` 中冻结的命令 ID 为 0 到 15：

| command_id | 拼音命令 | 语义 |
| --- | --- | --- |
| 0 | `da kai che deng` | 打开车灯 |
| 1 | `guan bi che deng` | 关闭车灯 |
| 2 | `jia su` | 加速 |
| 3 | `jian su` | 减速 |
| 4 | `sha che` | 刹车 |
| 5 | `ting che` | 停车 |
| 6 | `zuo zhuan` | 左转 |
| 7 | `you zhuan` | 右转 |
| 8 | `zuo diao tou` | 左掉头 |
| 9 | `you diao tou` | 右掉头 |
| 10 | `dao che` | 倒车 |
| 11 | `wei zhi mo shi` | 位置模式 |
| 12 | `su du mo shi` | 速度模式 |
| 13 | `zhuang tai cha xun` | 状态查询 |
| 14 | `chong qi xi tong` | 重启系统 |
| 15 | `chu shi hua` | 初始化 |

建议把这张映射表与 STM32 执行端仓库保持一致。

## 快速开始

### 环境

- ESP-IDF 5.3.x
- 目标芯片：`ESP32-S3`
- 依赖组件见 [main/idf_component.yml](main/idf_component.yml)

### 典型构建流程

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

### 发布前必须修改的配置

默认的 ESP-NOW 目标 MAC 已经改为公开安全的占位值：

- `CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC="FF:FF:FF:FF:FF:FF"`

在真正联调或部署前，请把它改成你的 ESP8266 桥接设备或目标节点 MAC 地址。可以通过 `menuconfig` 或直接修改 `sdkconfig.defaults.esp32s3` 完成。

## 文档

- [Architecture](docs/architecture.md)
- [Protocol](docs/protocol.md)
- [Integration](docs/integration.md)
- [ESP8266 Bridge](tools/esp8266_esnow_slave_bridge/README.md)

## 公开仓库建议

建议仓库名使用以下风格之一：

- `esp32_voice_motor_control_frontend`
- `offline_voice_motor_control_esp32`
- `esp32_voice_command_bridge`

公开前建议不要上传：

- `build/`
- `managed_components/`
- `sdkconfig`
- `sdkconfig.old`
- `.vscode/`
- `.claude/`

## 配套仓库建议

如果你同时公开执行端，推荐拆成：

- `stm32-foc-motor-controller`
- `esp32-voice-command-frontend`

两边 README 互相链接，并共用协议字段说明与 `command_id` 映射表。
