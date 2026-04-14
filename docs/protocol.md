# Protocol

## 总览

当前系统分成两段通信：

1. ESP32 前端 -> ESP8266 桥接节点：ESP-NOW 二进制帧
2. ESP8266 桥接节点 -> 下游 MCU：TTL 串口帧

两段链路都围绕同一组业务字段：

- `command_id`
- `seq`
- `prob_q15`
- `ts_ms`

## ESP-NOW 业务帧

定义位于 [components/espnow_bridge/include/espnow_bridge.h](../components/espnow_bridge/include/espnow_bridge.h)。

```c
typedef struct __attribute__((packed)) {
    uint8_t var;
    uint8_t msg_type;
    uint16_t seq;
    int16_t command_id;
    uint16_t prob_q15;
    uint32_t ts_ms;
} espnow_voice_cmd_t;
```

### 字段说明

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `var` | `uint8_t` | 协议版本，当前代码发送值为 `1` |
| `msg_type` | `uint8_t` | `0x01` 为语音命令，`0x02` 为心跳 |
| `seq` | `uint16_t` | 发送端递增序号，用于去重 |
| `command_id` | `int16_t` | 业务命令编号 |
| `prob_q15` | `uint16_t` | 识别置信度，Q15 格式，范围 `0..32767` |
| `ts_ms` | `uint32_t` | 发送时间戳，单位毫秒 |

### 当前约束

- 本地应用允许的命令范围：`0..15`
- ESP8266 桥接固件当前默认接受范围：`0..14`
- 远端桥接默认最小置信度：`16384`
- 心跳帧使用 `msg_type = 0x02`，`command_id = -1`

如果你准备长期公开维护，建议尽快把 ESP32 与 ESP8266 两侧的命令范围统一为同一份文档约束。

## TTL 串口帧

ESP8266 桥接固件会把有效的 ESP-NOW 命令转成如下串口格式：

```text
SOF(2) + LEN(1) + PAYLOAD(12) + XOR(1)
```

### 帧头

- `SOF0 = 0xA5`
- `SOF1 = 0x5A`
- `LEN = 12`

### PAYLOAD 布局

小端序：

| 偏移 | 字段 | 类型 | 说明 |
| --- | --- | --- | --- |
| 0 | `ver` | `uint8_t` | 协议版本，当前为 `1` |
| 1 | `source` | `uint8_t` | 当前桥接输出固定为 `1`，表示 remote ESPNOW |
| 2 | `command_id` | `int16_t` | 命令编号 |
| 4 | `seq` | `uint16_t` | 去重序号 |
| 6 | `prob_q15` | `uint16_t` | 识别置信度 |
| 8 | `ts_ms` | `uint32_t` | 时间戳 |

### 校验

- `CRC = XOR(LEN + PAYLOAD bytes)`

## 当前命令映射

当前 `sdkconfig.defaults.esp32s3` 中固定的命令表如下：

| command_id | 拼音命令 | 建议业务语义 |
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

建议在 STM32 仓库中用同一张表描述串口协议的 `command_id` 映射，避免双边口径漂移。
