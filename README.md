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

## Wiring

A simple wiring example is shown below.

| ATtiny85 pin | Function | Connection |
| --- | --- | --- |
| PB0 (pin 5) | RESET output | Connect to the Xiao RESET pin |
| PB1 (pin 6) | Heartbeat input | Connect to a heartbeat signal from the Xiao |
| PB2 (pin 7) | Battery ADC input | Connect to the battery divider midpoint |
| VCC (pin 8) | Supply | Connect to 3.3 V |
| GND (pin 4) | Ground | Connect to GND |

### Battery divider note

The firmware assumes a 1:2 resistor divider (for example 100 kΩ + 100 kΩ) from battery positive to ground, with the midpoint connected to PB2.

### Reset behavior

The ATtiny85 pulls the Xiao RESET line low for a short time to force a reboot when the watchdog conditions are met.

## Usage

1. Open the folder with PlatformIO or VS Code.
2. Build and upload the firmware to the ATtiny85.
3. Verify the wiring for reset, heartbeat, and battery ADC.
