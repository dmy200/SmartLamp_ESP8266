Here’s a draft for your `README.md` file in English:

---

# SmartLampESP8266

SmartLampESP8266 is a smart lamp controller project designed for ESP8266-based microcontrollers. It allows you to control a lamp via a web interface, schedule on/off timings, and synchronize time with NTP servers.

## Features

- **Wi-Fi Connectivity**: Connect the ESP8266-based controller to your Wi-Fi network.
- **Web Interface**: Intuitive HTML-based web interface for control and configuration.
- **Schedule Control**: Set on/off schedules for automatic operation.
- **NTP Synchronization**: Synchronize time using NTP servers.
- **PWM Support**: Smooth brightness adjustment for lamps supporting PWM.
- **Manual and Auto Modes**: Switch between manual and automatic operation modes.
- **Daylight Saving Time (DST)**: Automatic handling of summer and winter time.

## Hardware Requirements

- ESP8266-based microcontroller (e.g., NodeMCU, ESP-01, etc.).
- Lamp or LED connected to a GPIO pin of the ESP8266.
- Power supply for the ESP8266 and connected lamp.
- Optional: Additional components for enhanced functionality (e.g., resistors, transistors, etc.).

### Pin Configuration

- Lamp Pin: GPIO2 (can be configured in the code).

## Software Requirements

- Arduino IDE (or compatible IDE) with the following libraries installed:
  - `ESP8266WiFi`
  - `ESP8266WebServer`
  - `WiFiUdp`
  - `NTPClient`
  - `EEPROM`

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/dmy200/SmartLampESP8266.git
   ```
2. Open the `ESP8266_SmartLamp.ino` file in the Arduino IDE.
3. Install the required libraries using the Library Manager.
4. Configure your Wi-Fi credentials in the code:
   ```cpp
   const char* ssid = "your_wifi_ssid";
   const char* password = "your_wifi_password";
   ```
5. Upload the code to the ESP8266 microcontroller.

## Usage

1. After powering on the ESP8266, it will connect to the configured Wi-Fi network.
2. Access the web interface via the ESP8266's IP address (displayed in the Serial Monitor).
3. Use the web interface to:
   - Turn the lamp on/off manually.
   - Configure the schedule for automatic operation.
   - Save settings for future use.

### Web Interface Features

- **Status Display**: Current date, time, and lamp status.
- **Control Buttons**: Manual control options (`On`, `Off`, `Auto`).
- **Schedule Settings**: Configure on/off timings.
- **Advanced Settings**: Enable/disable PWM and invert signal logic.

## Code Overview

The main code is located in [`src/ESP8266_SmartLamp/ESP8266_SmartLamp.ino`](https://github.com/dmy200/SmartLampESP8266/blob/main/src/ESP8266_SmartLamp/ESP8266_SmartLamp.ino). It includes the following key sections:
- **Wi-Fi Connection**: Handles connection to the Wi-Fi network.
- **NTP Synchronization**: Synchronizes time using NTP servers.
- **Lamp Control**: Provides functions for controlling the lamp (manual, automatic, PWM fading).
- **Web Server**: Hosts the web interface for configuration and control.

## Known Issues

1. **Wi-Fi Credentials in Code**: Credentials are hardcoded, which is not secure. Consider implementing a configuration portal.
2. **EEPROM Usage**: Frequent writes to EEPROM may degrade its lifespan. Use with care.
3. **Time Zone Handling**: Currently, the time zone is fixed (UTC+2). Dynamic time zone management can be added.

## Future Improvements

- Add support for over-the-air (OTA) updates.
- Implement MQTT or other protocols for integration with smart home systems.
- Make the web interface more customizable.
- Add a configuration portal for Wi-Fi credentials.

## License

This project is licensed under the [MIT License](LICENSE).

---

Feel free to customize this further based on your preferences. Let me know if you’d like to refine or add anything!
