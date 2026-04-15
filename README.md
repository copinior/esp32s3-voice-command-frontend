# ESP32-S3 Voice Command Frontend

[中文说明](README.zh-CN.md)

## Demo Video

- Bilibili: https://www.bilibili.com/video/BV165QEBjEX6/

An offline voice-command frontend built on ESP32-S3. The project handles wake-word detection, command recognition, local UI feedback, and command forwarding over ESP-NOW to a bridge node or downstream controller.

## Overview

This repository implements the frontend side of an embedded voice-command system:

- offline speech recognition on ESP32-S3
- local runtime status display with LVGL
- ESP-NOW command forwarding
- optional ESP8266 bridge firmware for TTL UART output

Controller-side execution logic is outside the scope of this repository.

## Runtime Responsibilities

- wake-word and command recognition on ESP32-S3
- unified dispatch for local and remote command events
- runtime status display for command source, confidence, RSSI, and link state
- ESP-NOW transmission, receive handling, and heartbeat
- optional ESP8266 bridge support for wireless-to-UART forwarding

## Repository Layout

```text
main/                               application entry and speech pipeline
components/espnow_bridge/           ESP-NOW transport wrapper
components/hardware_driver/         board support and hardware abstraction
components/lvgl_ui/                 LVGL monitor UI
tools/esp8266_esnow_slave_bridge/   ESP8266 bridge firmware
docs/                               supplementary project notes
```

The current application path depends on:

- `hardware_driver`
- `espnow_bridge`
- `lvgl_ui`

## Processing Flow

The main runtime path is:

1. Audio data is captured through the board support layer
2. AFE / WakeNet / MultiNet perform wake-word and command recognition
3. Recognized commands enter a unified dispatch path
4. Command status is reported to the LVGL UI
5. Local commands are forwarded over ESP-NOW when required
6. Bridge or backend nodes consume the forwarded command packet

## Command Map

The current project configuration defines active command IDs `0..15`. Command IDs `0` and `1` are reserved for light-control behavior, while backend execution for these commands is not yet connected.

| command_id | phrase | meaning |
| --- | --- | --- |
| 0 | `da kai che deng` | turn on lights |
| 1 | `guan bi che deng` | turn off lights |
| 2 | `jia su` | accelerate |
| 3 | `jian su` | decelerate |
| 4 | `sha che` | brake |
| 5 | `ting che` | stop |
| 6 | `zuo zhuan` | turn left |
| 7 | `you zhuan` | turn right |
| 8 | `zuo diao tou` | U-turn left |
| 9 | `you diao tou` | U-turn right |
| 10 | `dao che` | reverse |
| 11 | `wei zhi mo shi` | position mode |
| 12 | `su du mo shi` | speed mode |
| 13 | `zhuang tai cha xun` | status query |
| 14 | `chong qi xi tong` | restart system |
| 15 | `chu shi hua` | initialize |

## Requirements

- ESP-IDF 5.3.x
- target: `ESP32-S3`
- dependencies: [main/idf_component.yml](main/idf_component.yml)

## Configuration

The default ESP-NOW destination MAC is set to a placeholder:

```text
CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC="FF:FF:FF:FF:FF:FF"
```

Replace it with the MAC address of the target ESP8266 bridge or peer node before deployment.
