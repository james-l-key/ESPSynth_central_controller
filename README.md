# ESP32 Modular Synthesizer - Central Controller Firmware

This directory contains the firmware for the **Central Controller Module** of the ESP32 Modular Synthesizer project.

## Role in the System

The Central Controller acts as the main "brain" and user interface hub for the synthesizer. Its primary responsibilities include:

* Hosting the main user interface (SSD1336 Color Display, encoders, buttons).
* Managing the virtual patch matrix for routing audio and modulation signals between modules.
* Acting as the I2C Master on the main control bus, communicating with all processing and I/O modules via TCA9548A multiplexers.
* Discovering and mapping connected modules.
* Handling external control inputs (OSC via Wi-Fi, MIDI via USB).
* Managing global system settings (e.g., sample rate).

## Target Hardware

* **MCU:** ESP32-S3-DevKitC-1-N32R8V (or compatible ESP32-S3 board with sufficient PSRAM and Flash)
* **Display:** SSD1336 Color OLED (connected via SPI)
* **Input:** Rotary encoders, Digital buttons (connected to GPIO pins)
* **Connectivity:** TCA9548A I2C Multiplexer(s) connected to the main I2C pins.

## Development Environment

* **Framework:** ESP-IDF (Espressif IoT Development Framework)
* **Version:** v5.4.1 (or later compatible version)

Ensure you have ESP-IDF v5.4.1 installed and configured correctly. Follow the official [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/get-started/index.html) for installation.

## Building and Flashing

1. **Set up ESP-IDF Environment:** Open your terminal and navigate to your ESP-IDF installation directory. Run the environment setup script:
    * Linux/macOS: `. ./export.sh`
    * Windows: `export.bat`

2. **Navigate to Firmware Directory:** Change directory to this `central_controller` folder:

    ```bash
    cd path/to/your/project/firmware/central_controller
    ```

3. **Set Target:** Ensure the target is set to ESP32-S3:

    ```bash
    idf.py set-target esp32s3
    ```

4. **Configure (Optional):** Open the configuration menu:

    ```bash
    idf.py menuconfig
    ```

    * Review settings under `Component config -> Central Controller Settings` (or similar).
    * Configure Wi-Fi credentials if needed (`Component config -> ESP WiFi`).
    * Verify pin assignments for display, inputs, and I2C.
    * Save and exit.

5. **Build:** Compile the firmware:

    ```bash
    idf.py build
    ```

6. **Flash:** Connect the Central Controller ESP32-S3 board via USB and upload the firmware (replace `/dev/ttyUSB0` or `COM3` with your board's serial port):

    ```bash
    idf.py -p /dev/ttyACM0 flash # Example port, adjust as needed
    ```

7. **Monitor:** View serial output:

    ```bash
    idf.py -p /dev/ttyACM0 monitor # Example port, adjust as needed
    ```

    (Press `Ctrl+]` to exit the monitor).

## Firmware Structure

This firmware follows the ESP-IDF component structure:

* **`main/`**: Contains the main application entry point (`app_main`) which initializes components and starts FreeRTOS tasks.
* **`components/`**: Contains functional blocks specific to the Central Controller:.
  * `i2c_manager`: Controls the main I2C bus (Master), TCA9548A multiplexer, and module communication protocol.
  * `patch_manager`: Manages the state of the virtual patch matrix.
  * `osc_handler`, `midi_handler`, `network_manager`, `usb_manager`: Handle respective communication protocols.
  * `global_settings`: Manages persistent settings using NVS.
  * `common_definitions`: Shared data types and constants within this firmware.
  * `Esp_menu`:AProject agnostic menu system with support for SSD1306 via I2C and rotary encoder(s)
* **Shared Components:** This firmware also depends on components located in the `firmware/shared_components/` directory (relative to the project root), primarily:
  * `module_i2c_proto`: Defines the I2C communication protocol used to talk to other modules.

The main `CMakeLists.txt` in this directory is configured to find these shared components.

## Status

*Work in Progress* - Core functionalities are under development. Things are subject to change with or without notice.
