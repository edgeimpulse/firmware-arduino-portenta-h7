# Edge Impulse firmware for Arduino Portenta H7

Edge Impulse enables developers to create the next generation of intelligent device solutions with embedded Machine Learning. This repository contains the Edge Impulse firmware for the Arduino Portena H7 development board. This device supports all Edge Impulse device features, including ingestion, remote management and inferencing.

> **Note:** Do you just want to use this development board with Edge Impulse? No need to build this firmware. See the instructions [here](https://docs.edgeimpulse.com/docs/arduino-portenta-h7) for a prebuilt image and instructions. Or, you can use the [data forwarder](https://docs.edgeimpulse.com/docs/cli-data-forwarder) to capture data from any sensor.

## Requirements

### Hardware

* [Arduino Portenta H7](https://store.arduino.cc/portenta-h7) development board.
* [Arduino Portenta Vision Shield (LoRa or Ethernet)](https://store.arduino.cc/portenta-vision-shield).

### Tools

The arduino-cli tool is used to build and upload the Edge Impulse firmware to the Arduino Portenta H7 board. Use following link for download and installation procedure:

* [Arduino CLI](https://arduino.github.io/arduino-cli/installation/).

The Edge Impulse firmware depends on some libraries and the Mbed core for Arduino. These will be automatically installed if you don't have them yet.

* Arduino IDE (required for Windows users)

_Installing Arduino IDE is a requirement only for Windows users. macOS and Linux users can use either the Arduino CLI or IDE to build the application._

1. Download and install the [Arduino IDE](https://www.arduino.cc/en/software) for your Operating System.
1. In Tools -> Board -> Boards Manager, search for `portenta` and install the **Arduino Mbed OS Portenta Boards v2.6.1**.

## Building the application

### With Arduino CLI

1. Build the application:

    ```
    ./arduino-build.sh --build
    ```

1. Flash the application:

    ```
    ./arduino-build.sh --flash
    ```

### Arduino IDE

1. Open the `firmware-arduino-portenta-h7.ino`, select the **Arduino Portenta H7 (M7 core)** board and the Flash Split **2 MB M7 + M4 in SDRAM**.
1. Build and flash the application using the **Upload** button.

## Using your own exported `Arduino` library from

Extract the contents of the exported `Arduino` library and replace `src/edge-impulse-sdk`, `src/tflite-model` and `src/model-parameters` in the project
accordingly.

## Troubleshooting

* Not flashing? You can double tap the button on the board to put it in bootloader mode.
* Invalid DFU suffix signature?

    ```
    dfu-util: Invalid DFU suffix signature
    dfu-util: A valid DFU suffix will be required in a future dfu-util release!!!
    dfu-util: Cannot open DFU device 2341:035b
    dfu-util: No DFU capable USB device available
    Upload error: Error: 2 UNKNOWN: uploading error: uploading error: exit status 74
    ```

    Having the above issues? Then copy `20-arduino.rules` to `/etc/udev/rules.d/` and try again.
