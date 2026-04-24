# E-Paper Smart Calendar
A low-power smart calendar device that displays upcoming events and weather information on an e-paper screen. Built around the ESP32-S3, the system is designed for long battery life, minimal power consumption, and reliable offline persistence.
Built around the ESP32-S3, the system is designed for long battery life, minimal power consumption, and reliable offline persistence.

## Features
- Google Calendar & weather integration: Fetches event data from the user's Google Calendar and weather forecast information from open-meteo.
  - Synced data is cached and saved to FRAM, avoiding the need to resync on every reboot, saving power.
- Ultra-low power system with aggressive power gating: The entire system is power gated using a P-MOSFET and a TPL5110, waking periodically to update the display.
  - Network sync is expensive in terms of power, and as such only happens every few automatic wakes.
  - WiFi remains disabled outside of sync cycles.
  - E-paper display retains the image without power
  - Powered by 4xAA batteries (designed for NiMH),
  - Will display warning and reduce power usage to minimum if voltage goes under a safe threshold.
> [!NOTE]
> These power saving techniques reduce the idle power usage to under a microampere, with short bursts of power consumption during a wake cycle.
> This increases battery life significantly.
- Interactive mode: The user can wake the device and browse events/forecasts for upcoming days.
- ESP SoftAP WiFi Provisioning: Network credentials can be configured from a mobile device using Espressif’s provisioning tools.
- Google API tokens are provisioned via serial connection and automatically refreshed when expired.
- Displays indoor temperature and humidity using onboard sensors.

## Key Components

* ESP32-S3 DevkitC-1
* Adafruit SHT40 breakout board (ADAFRUIT4885)
* 4MBit FRAM IC: MB85RS4MTPF-G-BCERE1, with a matching DIP adapter
* Pololu 3.3V step-down voltage regulator module DT24V10F3
* Waveshare 13353 4.2 inch E-Paper module
* TPL5110 Low Power Timer (with matching DIP adapter)
  
A full BOM can be found in the project report in the docs directory.

## Media

High-level system diagram:
<img width="544" height="449" alt="system-diagram" src="https://github.com/user-attachments/assets/c2fffdd7-6536-4cf1-9ea9-e44c2885c831" />
Software flow diagram:
<img width="2040" height="864" alt="flowidag" src="https://github.com/user-attachments/assets/5e7ae218-012a-4064-a2d2-9421ea8371d6" />

## Notes
Includes full schematic (.sch) and PCB layout (.brd) files.
Designed as a custom PCB and manufactured via JLCPCB.
All components were hand-soldered and the device was fully assembled and tested.
The /docs directory contains a detailed report with the BOM, assembly guide, and operating instructions.

