| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# _RISC-V Agent_

Agent to be installed on a ESP32-C3 Supermini (with integrated flash) and to be modified to create your own agent. This version makes random valid moves.

# Installation

- Install ESP-IDF from https://dl.espressif.com/dl/esp-idf/
- Install ESP IDF extension from VSCode Marketplace
- Configure ESP-IDF extension with Espressif Path (default: C:/Espressif/frameoworks/)
- Wait till ESP-IDF is completely finished installing
- Select current ESP-IDF version: the one installed
- Select Flash Method: UART
- Select Port: the one where ESP is connected
- Select Device Target: ESP32-C3
- Create a config.h like described below
- Build, Flash & Enjoy!

# Config files

Add a config file *next to* `main.c`. Name it `config.h`

Contents:
```
#ifndef CONFIG_H
#define CONFIG_H

#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASS "WIFI_PASS"

#define AGENTNAME "Name"

#endif
```
`AGENTNAME` has to be 4 characters.