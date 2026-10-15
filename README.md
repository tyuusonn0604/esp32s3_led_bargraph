# ESP32-S3 LED Bar Graph Controller

ESP32-S3とESP-IDFを使用した、8個のLEDによる残光付きランニングライト制御プロジェクトです。

## Features

- LEDCによるPWM制御（8-bit resolution）
- 8個のLEDの個別輝度制御
- PWMデューティ比を利用した残光表現
- ボタン操作によるアニメーションの開始・停止
- 停止時の全LED消灯
- ボタン入力のチャタリング対策

## Tech Stack

- Language: C
- Framework: ESP-IDF (v6.1)
- Target MCU: ESP32-S3

## Hardware

- MCU: ESP32-S3
- Development framework: ESP-IDF (v6.1)
- LED output: GPIO 42, 41, 40, 39, 38, 47, 21, 20
- Button input: GPIO 13
- PWM controller: LEDC

## Implementation

- PWM frequency: 5 kHz
- PWM resolution: 8 bit
- Button polling interval: 10 ms
- LED animation update interval: 100 ms

## Build

```powershell
idf.py build
```

## Flash and Monitor

```powershell
idf.py -p COM3 flash monitor
```

Replace `COM3` with the serial port assigned to your development board.

## Learning Objectives

- GPIO input/output control
- PWM generation using the LEDC peripheral
- Periodic processing with FreeRTOS
- Button input handling and debounce
- Managing multiple PWM channels

## Future Improvements

- Separate button monitoring and LED animation into FreeRTOS tasks
- Add unit tests for pattern generation
- Document the circuit and timing behavior
