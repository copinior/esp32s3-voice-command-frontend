# ESP32-S3 离线语音命令前端

[English README](README.md)

这是一个基于 ESP32-S3 的离线语音命令前端项目，负责完成唤醒词检测、命令词识别、本地 UI 状态反馈，以及通过 ESP-NOW 将命令下发到桥接节点或后端控制器。

## 项目定位

这个仓库聚焦“语音识别前端”职责：

- 在 ESP32-S3 本地完成离线语音识别
- 通过 LVGL 提供实时状态显示
- 通过 ESP-NOW 向外发送控制命令
- 配合 ESP8266 桥接固件输出 TTL 串口协议

执行端控制逻辑，例如电机闭环、状态机、安全约束等，不在本仓库中实现，更适合放在独立的后端控制器仓库中维护。

## 主要功能

- 唤醒词 + 命令词识别链路
- 本地命令与远端命令统一调度
- 基于 LVGL 的运行状态监控界面
- ESP-NOW 发送、接收、心跳机制
- ESP8266 桥接固件，用于无线到串口的协议转发

## 系统链路

```text
Microphone / Codec
    -> hardware_driver
    -> AFE / WakeNet / MultiNet
    -> command_dispatch_task
    -> lvgl_ui
    -> espnow_bridge
    -> ESP8266 bridge or backend controller
```

## 仓库结构

```text
main/                               应用入口与语音识别主流程
components/espnow_bridge/           ESP-NOW 通信封装
components/hardware_driver/         板级 BSP 与硬件抽象
components/lvgl_ui/                 LVGL 监控界面
tools/esp8266_esnow_slave_bridge/   ESP8266 桥接固件
docs/                               架构、协议与集成说明
```

当前主运行链路依赖的核心模块只有：

- `hardware_driver`
- `espnow_bridge`
- `lvgl_ui`

## 命令集

当前工程配置中的有效命令 ID 为 `0..15`：

| command_id | 命令词 | 语义 |
| --- | --- | --- |
| 0 | `da kai che deng` | 打开灯光 |
| 1 | `guan bi che deng` | 关闭灯光 |
| 2 | `jia su` | 加速 |
| 3 | `jian su` | 减速 |
| 4 | `sha che` | 刹车 |
| 5 | `ting che` | 停止 |
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

## 依赖环境

- ESP-IDF 5.3.x
- 目标芯片：`ESP32-S3`
- 依赖定义见 [main/idf_component.yml](main/idf_component.yml)

## 配置说明

默认 ESP-NOW 目标 MAC 当前设置为安全占位值：

```text
CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC="FF:FF:FF:FF:FF:FF"
```

在实际联调或部署前，应将其替换为目标桥接节点或目标对端设备的真实 MAC 地址。

## 文档入口

- [项目结构说明](PROJECT_STRUCTURE.md)
- [系统架构](docs/architecture.md)
- [通信协议](docs/protocol.md)
- [系统集成](docs/integration.md)
- [显示初始化记录](docs/display_init_lichuang_dev_step_by_step.md)
- [ESP8266 桥接固件说明](tools/esp8266_esnow_slave_bridge/README.md)
