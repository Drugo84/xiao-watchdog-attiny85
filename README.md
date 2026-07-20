# Xiao Watchdog ATtiny85 🛡️🔋

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ATtiny85-blue.svg)](https://platformio.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Firmware for ATtiny85 acting as an ultra-low-power hardware watchdog for Seeed Xiao nRF52840 nodes in off-grid Meshtastic setups.

##  Features

- 🔋 **Battery monitoring** with hysteresis (prevents rapid on/off cycling)
- 💓 **Heartbeat detection**: resets Xiao if no signal received for ~10 minutes
- 🔁 **Controlled hardware reset**: clean reboot of the node on software freeze
- 😴 **Deep sleep**: average consumption ~8-15 µA, solar-compatible
- 🧠 **SuperSmartReset logic**: shutdown at 3.2V, restart at 3.5V with stabilization delay

## 📐 Wiring Diagram ("Dead Bug" Style)

```
Seeed Xiao nRF52840           ATtiny85 SOIC-8 (top view, notch left)
─────────────────           ───────────────────────────────────────
VCC 3.3V ─────────────►       Pin 8 (VCC)
GND ─────────────────►        Pin 4 (GND)

Pin RESET ◄────────────────   Pin 5 (PB0)  // Reset line (shared w/ button)
                              │
Any GPIO (e.g. D5) ──────►    Pin 6 (PB1)  // Heartbeat input (toggle every 2-3s)

LiPo Vbat ──[100kΩ]──┬──[100kΩ]── GND
                     │
                   Pin 7 (PB2/A1)  // ADC battery sense

Pin 1 (RESET ATtiny) ──[10kΩ]──► VCC  // ISP pull-up (optional)
Pin 8 (VCC) ──||── Pin 4 (GND)        // 100nF decoupling cap (close to pins)
```

### 🔋 Battery Divider

| Component | Value | Notes |
|-----------|-------|-------|
| R1 (top) | 100kΩ 0603 | Between Vbat(+) and Pin 7 |
| R2 (bottom) | 100kΩ 0603 | Between Pin 7 and GND |

**ADC formula** (with `analogReference(INTERNAL)` = 1.1V reference):
```
ADC = (Vbat / 2) / 1.1V × 1023
```

| Real Vbat | Voltage on Pin 7 | ADC Value | Action |
|-----------|-----------------|-----------|--------|
| ≥ 3.50V | ≥ 1.75V | ≥ 800 | ✅ System ON / Restart after wait |
| 3.20-3.50V | 1.60-1.75V | 750-800 | 🔄 Keep previous state (hysteresis) |
| ≤ 3.20V | ≤ 1.60V | ≤ 750 | 🔴 System OFF → Reset Xiao |

## ⚙️ Configurable Parameters (`src/main.cpp`)

```cpp
// Battery thresholds (ADC with 1:2 divider, Vref=1.1V)
const int THRESH_LOW   = 750;  // ~3.2V → SHUTDOWN
const int THRESH_HIGH  = 800;  // ~3.5V → RESTART after wait

// Timing (hardware watchdog ~8 seconds per cycle)
const uint8_t WDT_CYCLES_DEAD     = 75;  // ~10 min no heartbeat → reset
const uint8_t WDT_CYCLES_RECOVERY = 76;  // ~10 min wait before restart
```

## 💓 Heartbeat from Xiao (Meshtastic Firmware)

The Xiao must send a "I'm alive" signal by toggling a GPIO:

```cpp
// In the Xiao firmware loop() (nRF52840)
#define HEARTBEAT_PIN 5  // D5 or any free GPIO

// Toggle every 2-3 seconds (NOT 10s!):
if (millis() - lastHeartbeat >= 3000) {
  lastHeartbeat = millis();
  digitalWrite(HEARTBEAT_PIN, !digitalRead(HEARTBEAT_PIN));  // Toggle state
}
```

> ⚠️ **Important**: Use a **2-3 second interval**. The ATtiny wakes every ~8 seconds: if you toggle every 10s, it may miss the change and trigger a false reset.

## 🔌 Reset Behavior

The ATtiny drives the Xiao RESET pin in **simulated open-drain mode**:

| ATtiny Pin 5 State | Xiao RESET Line State | Result |
|-------------------|----------------------|--------|
| `INPUT` (high-Z) | Internal pull-up (~40kΩ) pulls to ~3.3V | ✅ Xiao RUN |
| `OUTPUT` + `LOW` for ~100ms | Line pulled to 0V | 🔄 Xiao RESET |

✅ **No external pull-up resistor required**: the nRF52840 already has an internal pull-up on its RESET pin.

## 🔋 Estimated Power Consumption

| State | Current | Notes |
|-------|---------|-------|
| Deep sleep (PWR_DOWN) | ~5-10 µA | Watchdog active, ADC off |
| Wake + ADC read | ~200-300 µA | For ~10 ms every 8 seconds |
| **Real-world average** | **~8-15 µA** | Solar-node compatible |

**Theoretical autonomy** (1000mAh LiPo, no solar panel):
```
1000 mAh / 0.015 mA ≈ 66,000 hours ≈ 7.5 years
```
*(In practice: losses, self-discharge, LoRa transmissions reduce this to ~6-12 months)*

## 🚀 Build & Upload

```bash
# Requirements: PlatformIO, avrdude, ISP programmer (e.g. USBasp)

# 1. Set fuses for 8 MHz internal clock (one-time):
avrdude -c usbasp -p t85 -U lfuse:w:0xE1:m

# 2. Compile and upload:
pio run -t upload
```

**Reference `platformio.ini`**:
```ini
[env:attiny85]
platform = atmelavr
board = attiny85
framework = arduino
upload_protocol = avrispmkII
board_build.f_cpu = 8000000L
build_flags = -Os
upload_flags = -B 32  ; Slow ISP for flying leads
```

##  Quick Testing

### Bench test (without Xiao connected)
1. Power ATtiny at 3.9V, connect a potentiometer to Pin 7 (instead of fixed divider)
2. Monitor Pin 5 with multimeter or LED+resistor:
   - After boot: Pin 5 floating (~0V on meter, normal)
   - Turn pot below threshold (ADC < 750) → after ~8s you'll see a 100ms LOW pulse
   - Turn pot above threshold (ADC > 800) → after ~10 minutes (76 cycles) you'll see a restart pulse

### With Xiao connected
1. Connect Pin 5 ATtiny → Xiao RESET, Xiao GPIO → Pin 6 ATtiny
2. Flash Meshtastic firmware with heartbeat every 2-3s on that GPIO
3. Simulate low battery (potentiometer divider): Xiao shuts down at ~3.2V
4. Recharge battery: Xiao restarts only after ~10 minutes of stable >3.5V
5. Disconnect heartbeat for 10 minutes: Xiao resets automatically

## 🔧 Troubleshooting

| Symptom | Possible Cause | Solution |
|---------|---------------|----------|
| Upload fails with "verification error" | Fuses not set, baud too high | Set fuse 0xE1, use `-B 32` in upload_flags |
| ADC always reads ~512 | `analogReference(DEFAULT)` instead of `INTERNAL` | Use `analogReference(INTERNAL)` |
| Continuous reset cycling | Thresholds too close, heartbeat too slow | Use hysteresis 750/800, toggle heartbeat every 2-3s |
| Pin 5 always 0V on meter | Pin in INPUT (high-Z), meter reads leakage | Normal: connect Xiao or add 10k pull-up for testing |
| Xiao doesn't reset when expected | RESET wiring inverted, wrong pin | Verify: Xiao RESET is active-low, Pin 5 ATtiny → RESET Xiao |

## 📄 License

MIT License – see [LICENSE](LICENSE) file for details.

---

> 💡 **Pro-tip**: For solar nodes, use **1MΩ resistors** in the divider instead of 100kΩ. Divider current drops from ~20 µA to ~2 µA. Remember to recalibrate ADC thresholds if you change values (very high impedance may introduce noise).
