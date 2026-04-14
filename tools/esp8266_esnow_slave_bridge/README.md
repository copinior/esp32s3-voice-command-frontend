# ESP8266 ESPNOW 从机 -> TTL 转发桥

本目录是 **Arduino 固件**（不是 AT 固件），用于：

- 作为 ESP-NOW 从机接收 `espnow_voice_cmd_t`
- 做协议校验、去重、节流
- 通过 TTL 串口转发到下游 MCU

## 已对齐发送端协议

- 结构体：`espnow_voice_cmd_t`（12 字节 packed）
  - `var` (`uint8_t`)
  - `msg_type` (`uint8_t`)
  - `seq` (`uint16_t`)
  - `command_id` (`int16_t`)
  - `prob_q15` (`uint16_t`)
  - `ts_ms` (`uint32_t`)
- `msg_type == 0x01`
- `command_id` 范围 `[0,14]`
- `prob_q15 >= 16384`

## 串口参数映射（对应你提供的模块参数）

在 `esp8266_esnow_slave_bridge.ino` 顶部可配置：

- 波特率：`DEBUG_BAUD`、`TTL_BAUD`
- 数据位/停止位/校验位：`DEBUG_SERIAL_CFG`、`TTL_SERIAL_CFG`
  - 例如：`SERIAL_8N1`、`SERIAL_8E1`、`SERIAL_7O2`

说明：

- 当前默认 `115200, 8N1`
- 流控（RTS）在本方案未启用
- 若切到 `SoftwareSerial`，建议仅用 `8N1`

## IRAM 优化策略（当前默认）

- `TTL_USE_SOFTWARE_SERIAL = 0`（默认）
  - TTL 走 `UART1 TX(GPIO2)`，显著减小 IRAM 压力
- `DEBUG_LOG_ENABLE` 可置 `0`
  - 关闭调试日志，进一步减小代码体积与运行开销

## TTL 帧格式

输出帧：`SOF(2) + LEN(1) + PAYLOAD(12) + XOR(1)`

- `SOF0 = 0xA5`
- `SOF1 = 0x5A`
- `LEN = 12`
- `PAYLOAD`（小端）
  - `ver` (`uint8_t`) = `1`
  - `source` (`uint8_t`) = `1`（remote ESPNOW）
  - `command_id` (`int16_t`)
  - `seq` (`uint16_t`)
  - `prob_q15` (`uint16_t`)
  - `ts_ms` (`uint32_t`)
- `CRC = XOR(LEN + PAYLOAD bytes)`

## 接线提示

- 模块是 LVTTL 电平，不能直连 RS232
- TXD 接下游 RXD，RXD 接下游 TXD
- 共地（GND）必须连接

## AT 固件与本工程区别

你给的 AT 指令参数（`AT+...`）适用于 **ESP8266 AT 固件**。
本工程是 **Arduino 自定义固件**，不走 `AT` 指令集。

如果你要改成“主控发 AT 指令 + ESP8266 透传模块”的架构，需要单独换 AT 固件方案。
