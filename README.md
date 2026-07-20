# Xiao Watchdog ATtiny85

Local project for an ATtiny85 watchdog dedicated to a Seeed Xiao nRF52840 used in a Meshtastic setup.

## Purpose

The firmware monitors:
- the battery status of the system;
- the heartbeat coming from the Xiao;
- the reset of the remote microcontroller in case of a lock-up or loss of signal.

## Architecture

- ATtiny85: low-power supervisor
- Xiao nRF52840: main device to protect
- Reset through a dedicated pin
- Deep sleep to reduce power consumption

## Development notes

The project is intended to be compiled with PlatformIO targeting the ATtiny85.

## Main structure

- src/main.cpp: main firmware
- platformio.ini: PlatformIO configuration

## Usage

1. Open the folder with PlatformIO or VS Code.
2. Build and upload the firmware to the ATtiny85.
3. Verify the wiring for reset, heartbeat, and battery ADC.
