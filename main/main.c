#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "i2c_manager.h"
#include "patch_manager.h"
#include "synth_constants.h" // From common_definitions

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Central Controller Firmware");

    // Initialize NVS (Non-Volatile Storage) - required by many components
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Core Components
    ESP_LOGI(TAG, "Initializing I2C Manager...");

    // Create and configure the I2C manager configuration
    i2c_manager_config_t i2c_config = {
        .scl_pin = 22,        // Example: SCL on GPIO22
        .sda_pin = 21,        // Example: SDA on GPIO21
        .clock_speed = 400000 // Example: 400 KHz
    };

    ret = i2c_manager_init(&i2c_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C Manager!");
        // Handle critical failure - perhaps restart or halt
        return;
    }
    else
    {
        ESP_LOGI(TAG, "I2C Manager Initialized.");
    }

    ESP_LOGI(TAG, "Initializing Patch Manager...");
    ret = patch_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Patch Manager!");
        // Handle critical failure
        return;
    }
    else
    {
        ESP_LOGI(TAG, "Patch Manager Initialized.");
    }

    // --- Initialization Complete ---
    ESP_LOGI(TAG, "System Initialization Complete.");

    // --- Placeholder Task/Loop ---
    // In a real application, you would start tasks here for UI, networking, etc.
    // For now, just print a message periodically.
    int module_count = 0; // Placeholder
    while (1)
    {
        ESP_LOGI(TAG, "Main loop running... Discovered Modules (Placeholder): %d", module_count);

        // Example: Periodically select different mux channels and probe an address
        for (uint8_t i = 0; i < MAX_I2C_MUX_CHANNELS; i++)
        {
            ESP_LOGD(TAG, "Selecting MUX channel %d", i);
            ret = i2c_manager_select_mux_channel(i);
            if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to select MUX channel %d", i);
            }
            else
            {
                // Placeholder: Try to probe a device on the selected channel
                uint8_t dummy_addr = 0x50; // Example address
                ret = i2c_manager_probe_device(dummy_addr);
                if (ret == ESP_OK)
                {
                    ESP_LOGI(TAG, "Device ACKed at 0x%02X on channel %d", dummy_addr, i);
                }
                else
                {
                    ESP_LOGD(TAG, "No ACK from 0x%02X on channel %d", dummy_addr, i);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between probes
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Delay for 5 seconds
    }
}