# ESP32-S3 离线语音命令前端

[English README](README.md)

## 演示视频

- Bilibili: https://www.bilibili.com/video/BV165QEBjEX6/

此项目基于 ESP32-S3 的离线语音命令前端项目，负责完成唤醒词检测、命令词识别、本地 UI 状态反馈，以及通过 ESP-NOW 将命令发送到桥接节点或后端控制器。

## 项目定位

项目意在实现嵌入式语音命令系统中的前端部分：

- 在 ESP32-S3 本地完成离线语音识别
- 通过 LVGL 提供运行状态显示
- 通过 ESP-NOW 向外发送控制命令
- 配合 ESP8266 桥接固件输出 TTL 串口协议

执行端控制逻辑在另外的项目中实现。

## 运行职责

- 完成唤醒词与命令词识别
- 统一调度本地命令和远端命令事件
- 显示命令来源、置信度、RSSI 和链路状态
- 处理 ESP-NOW 发送、接收和心跳
- 支持 ESP8266 无线到串口桥接

## 仓库结构

```text
main/                               应用入口与语音识别主流程
components/espnow_bridge/           ESP-NOW 通信封装
components/hardware_driver/         板级 BSP 与硬件抽象
components/lvgl_ui/                 LVGL 监控界面
tools/esp8266_esnow_slave_bridge/   ESP8266 桥接固件
docs/                               补充项目说明
```

主运行链路依赖的核心模块为：

- `hardware_driver`
- `espnow_bridge`
- `lvgl_ui`

## 处理流程

主流程为：

1. 通过板级驱动获取音频数据
2. 由 AFE / WakeNet / MultiNet 完成唤醒与命令识别
3. 识别结果进入统一命令调度路径
4. 将命令状态反馈到 LVGL 界面
5. 按需通过 ESP-NOW 向外发送命令
6. 由桥接节点或后端控制器接收并继续处理

## 命令集

工程配置中的有效命令 ID 为 `0..15`。其中命令 `0` 和 `1` 预留为灯光控制命令，当前后端执行逻辑尚未接入。

| command_id | 命令词 | 语义 |
| --- | --- | --- |
| 0 | `da kai che deng` | 打开灯光 |
| 1 | `guan bi che deng` | 关闭灯光 |
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

## 依赖环境

- ESP-IDF 5.3.x
- 目标芯片：`ESP32-S3`
- 依赖定义见 [main/idf_component.yml](main/idf_component.yml)

## 配置说明

默认 ESP-NOW 目标 MAC 设置为占位值：

```text
CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC="FF:FF:FF:FF:FF:FF"
```

实际联调或部署时替换为目标桥接节点或目标对端设备的真实 MAC 地址。
