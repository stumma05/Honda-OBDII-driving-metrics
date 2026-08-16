# Honda Earth Dreams CAN Bus Monitor
This project uses an ESP32 microcontroller to interface with a Honda vehicle's CAN bus, specifically reverse-engineering powertrain data for Earth Dreams engines. It hosts a local web server to display real-time driving metrics on a custom mobile-friendly dashboard.  
# Features
- Passive CAN Sniffing: Seamlessly reads broadcast frames for immediate engine speed (RPM) and vehicle speed (km/h).
- Active UDS Polling: Initiates a diagnostic session with the Powertrain Control Unit (PCU) to request extended data.
- Multi-frame Handling: Successfully reassembles multi-frame UDS payloads to process complex diagnostic tables.
- Live Web Dashboard: Hosts an asynchronous JSON endpoint and a dark-themed HTML interface.
- Dynamic UI Elements: Interface cards change color based on conditions, such as mechanical lock detection for the Torque Converter Clutch (TCC).
# Captured Metrics
The ESP32 successfully decodes and displays the following data points:
- Engine Speed: Extracted passively from CAN ID 0x17C.
- Vehicle Speed: Extracted passively from CAN ID 0x158.
- Coolant & CVT Fluid Temperatures: Queried actively from PID 0x3004.
- Engine Load: Queried actively from PID 0x3004.
- CVT Pulley Speeds: Primary and secondary pulley RPM queried from PID 0x2301.
- CVT Gear Ratio: Calculated in real-time by comparing the primary and secondary pulley speeds.
- Torque Converter Slip: Calculated by finding the difference between the engine RPM and the primary pulley RPM.
# Hardware Requirements
- Microcontroller: ESP32 development board.
- CAN Transceiver: A 3.3V CAN transceiver module (e.g., SN65HVD230) connected to the vehicle's OBD-II port.
- Wiring:
  - Transceiver TX connects to ESP32 GPIO_NUM_6.
  - Transceiver RX connects to ESP32 GPIO_NUM_7.
# Software Dependencies
- Platform: Arduino IDE or PlatformIO.
- Built-in Libraries: <WiFi.h> and <WebServer.h> for network management and hosting.
- CAN Driver: ESP-IDF's native Two-Wire Automotive Interface (driver/twai.h) configured for 500 kbps.
# Usage Instructions
- Compile and upload the sketch to your ESP32 via the Arduino IDE.
- Plug the device into the vehicle's diagnostic port and turn the vehicle's ignition on.
- Using a smartphone or laptop, connect to the ESP32's Wi-Fi network: SSID: CRV_Diagnostic_Scanner  Password: hondapassword
- Open a web browser and navigate to the ESP32's local IP address (typically 192.168.4.1) to view the live dashboard. The UI will automatically fetch new metrics every 150 milliseconds.  
