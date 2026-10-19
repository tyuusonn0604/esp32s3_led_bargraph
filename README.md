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


## Software Architecture

The application uses two FreeRTOS tasks to separate button input handling from LED control.

### Tasks

- **Button Task**
  - Polls the button every 10 ms
  - Applies a 30 ms debounce interval
  - Sends a toggle event to the LED task through a FreeRTOS queue

- **LED Task**
  - Receives button events through the queue
  - Toggles the animation state
  - Updates the LED PWM duty cycles every 100 ms while running
  - Turns all LEDs off when stopped

### Inter-task Communication

A FreeRTOS queue is used to transfer button events from the button task to the LED task. The LED task owns the animation state and PWM updates, avoiding direct concurrent modification of LED control state.

### Key Learning Points

- FreeRTOS task creation and scheduling
- Queue-based inter-task communication
- Button debouncing
- PWM control using the ESP-IDF LEDC driver
- Periodic processing and state management
