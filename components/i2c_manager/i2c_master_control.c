#include "i2c_manager.h"
#include "driver/i2c_master.h" // New I2C Master Driver header
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" // For mutex
#include "synth_constants.h" // From common_definitions
// Include the shared protocol definitions (even if just for types initially)
#include "module_i2c_proto.h" // Assumed to exist in shared_components
#include <string.h>
static const char *TAG = "I2C_MANAGER";

// Configuration values (read from sdkconfig)
#define I2C_MASTER_PORT_NUM CONFIG_CENTRAL_I2C_MASTER_PORT_NUM
#define I2C_MASTER_SCL_IO CONFIG_CENTRAL_I2C_MASTER_SCL_IO
#define I2C_MASTER_SDA_IO CONFIG_CENTRAL_I2C_MASTER_SDA_IO
#define I2C_MASTER_FREQ_HZ CONFIG_CENTRAL_I2C_MASTER_FREQ_HZ
#define I2C_MUX_ADDR CONFIG_CENTRAL_I2C_MUX_ADDRESS

// State
static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t mux_dev_handle = NULL; // Device handle for the MUX itself
static SemaphoreHandle_t i2c_mutex = NULL;
static uint8_t current_mux_channel = 0xFF; // Invalid channel initially

// --- Initialization ---

esp_err_t i2c_manager_init(void)
{
    esp_err_t ret = ESP_OK;

    // Create Mutex for thread safety
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create I2C mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Initializing I2C Master Port: %d", I2C_MASTER_PORT_NUM);
    ESP_LOGI(TAG, "SCL Pin: %d, SDA Pin: %d, Freq: %d Hz", I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO, I2C_MASTER_FREQ_HZ);

    // Configure the I2C master bus
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT, // Use default clock source
        .i2c_port = I2C_MASTER_PORT_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,               // Default glitch filter setting
        .flags.enable_internal_pullup = true, // Enable internal pullups
    };
    ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        goto init_fail;
    }

    // Add the MUX as a device on the bus
    i2c_device_config_t mux_dev_cfg = {
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_MUX_ADDR, // Changed from dev_addr to device_address
    };
    ret = i2c_master_bus_add_device(bus_handle, &mux_dev_cfg, &mux_dev_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add MUX device (0x%02X) to bus: %s", I2C_MUX_ADDR, esp_err_to_name(ret));
        goto init_fail;
    }

    ESP_LOGI(TAG, "I2C Master bus and MUX device initialized successfully.");

    // Initial MUX state: Select channel 0 (or deselect all if preferred)
    ret = i2c_manager_select_mux_channel(0); // Select channel 0 initially
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Initial MUX channel selection failed: %s", esp_err_to_name(ret));
        // Continue initialization? Or return error? Depends on requirements.
        // For now, log error but continue.
    }
    else
    {
        ESP_LOGI(TAG, "I2C MUX Initialized, channel 0 selected.");
    }

    return ESP_OK;

init_fail:
    // Cleanup on failure
    if (mux_dev_handle)
    {
        i2c_master_bus_rm_device(mux_dev_handle);
        mux_dev_handle = NULL;
    }
    if (bus_handle)
    {
        i2c_del_master_bus(bus_handle);
        bus_handle = NULL;
    }
    if (i2c_mutex)
    {
        vSemaphoreDelete(i2c_mutex);
        i2c_mutex = NULL;
    }
    return ret;
}

esp_err_t i2c_manager_deinit(void)
{
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS * 2)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for deinit");
        // Proceed with deinit anyway, might be stuck
    }

    if (mux_dev_handle)
    {
        i2c_master_bus_rm_device(mux_dev_handle);
        mux_dev_handle = NULL;
    }
    if (bus_handle)
    {
        i2c_del_master_bus(bus_handle);
        bus_handle = NULL;
    }

    if (i2c_mutex)
    {
        xSemaphoreGive(i2c_mutex); // Give it back before deleting
        vSemaphoreDelete(i2c_mutex);
        i2c_mutex = NULL;
    }
    ESP_LOGI(TAG, "I2C Manager deinitialized.");
    return ESP_OK;
}

// --- TCA9548A MUX Control ---

esp_err_t i2c_manager_select_mux_channel(uint8_t channel)
{
    if (channel >= MAX_I2C_MUX_CHANNELS)
    {
        ESP_LOGE(TAG, "Invalid MUX channel: %d (Max is %d)", channel, MAX_I2C_MUX_CHANNELS - 1);
        return ESP_ERR_INVALID_ARG;
    }
    if (!bus_handle || !mux_dev_handle)
    {
        ESP_LOGE(TAG, "I2C Manager not initialized for MUX select");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS * 2)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for MUX select");
        return ESP_ERR_TIMEOUT;
    }

    // Only write to MUX if the channel is actually changing
    if (channel == current_mux_channel)
    {
        ret = ESP_OK; // Already on the correct channel
    }
    else
    {
        uint8_t write_buf = 1 << channel; // TCA9548A control register value

        // Use the new transmit function with the MUX device handle
        ret = i2c_master_transmit(mux_dev_handle, &write_buf, 1, I2C_TIMEOUT_MS);

        if (ret == ESP_OK)
        {
            ESP_LOGD(TAG, "Successfully selected MUX channel %d", channel);
            current_mux_channel = channel;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to select MUX channel %d: %s", channel, esp_err_to_name(ret));
            current_mux_channel = 0xFF; // Mark channel as unknown/invalid on error
        }
    }

    xSemaphoreGive(i2c_mutex);
    return ret;
}

// --- Module Communication ---

esp_err_t i2c_manager_send_command(uint8_t mux_channel, uint8_t module_address, uint8_t command_id, const void *data, size_t data_len)
{
    esp_err_t ret;

    if (!bus_handle)
    {
        ESP_LOGE(TAG, "I2C Manager not initialized for send command");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS * 2)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for send command");
        return ESP_ERR_TIMEOUT;
    }

    // 1. Select the correct MUX channel
    ret = i2c_manager_select_mux_channel(mux_channel); // This function handles mutex internally but we hold it across the whole operation
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to select MUX channel %d before sending command", mux_channel);
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    // 2. Perform the I2C Write Transaction
    // We need a temporary buffer to hold command_id + data
    uint8_t *tx_buffer = NULL;
    size_t total_len = 1 + data_len; // command_id + data
    if (total_len > 0)
    {   // Allocate only if there's something to send
        tx_buffer = (uint8_t *)malloc(total_len);
        if (tx_buffer == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate buffer for send command");
            xSemaphoreGive(i2c_mutex);
            return ESP_ERR_NO_MEM;
        }
        tx_buffer[0] = command_id;
        if (data != NULL && data_len > 0)
        {
            memcpy(tx_buffer + 1, data, data_len);
        }
    }
    else
    {
        ESP_LOGW(TAG, "Send command called with no command ID or data?");
        free(tx_buffer);
        xSemaphoreGive(i2c_mutex);
        return ESP_ERR_INVALID_ARG;
    }

    // Use i2c_master_transmit - creates a temporary device handle
    // This avoids needing to add/remove every module to the bus constantly.
    i2c_device_config_t module_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = module_address,   // Changed from dev_addr to device_address
        .scl_speed_hz = I2C_MASTER_FREQ_HZ, // Use bus default speed
    };

    ESP_LOGD(TAG, "Sending %d bytes (Cmd: 0x%02X) to MUX %d Addr 0x%02X", total_len, command_id, mux_channel, module_address);

    // Create a temporary device handle for this operation
    i2c_master_dev_handle_t temp_dev_handle = NULL;
    ret = i2c_master_bus_add_device(bus_handle, &module_dev_cfg, &temp_dev_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create temporary device handle: %s", esp_err_to_name(ret));
        free(tx_buffer);
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    // Transmit to the device
    ret = i2c_master_transmit(temp_dev_handle, tx_buffer, total_len, I2C_TIMEOUT_MS);

    // Remove the temporary device
    i2c_master_bus_rm_device(temp_dev_handle);

    free(tx_buffer); // Free the temporary buffer

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send command 0x%02X to 0x%02X on MUX %d: %s",
                 command_id, module_address, mux_channel, esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGD(TAG, "Command sent successfully.");
    }

    xSemaphoreGive(i2c_mutex);
    return ret;
}

esp_err_t i2c_manager_read_data(uint8_t mux_channel, uint8_t module_address, uint8_t request_id,
                                bool write_request_id, void *buffer, size_t buffer_len, size_t *bytes_read)
{
    esp_err_t ret;

    if (buffer == NULL || bytes_read == NULL || buffer_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *bytes_read = 0; // Initialize output

    if (!bus_handle)
    {
        ESP_LOGE(TAG, "I2C Manager not initialized for read data");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS * 2)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for read data");
        return ESP_ERR_TIMEOUT;
    }

    // 1. Select the correct MUX channel
    ret = i2c_manager_select_mux_channel(mux_channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to select MUX channel %d before reading data", mux_channel);
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    // 2. Perform the I2C Read Transaction
    i2c_device_config_t module_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = module_address, // Changed from dev_addr to device_address
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    // Create a temporary device handle for this operation
    i2c_master_dev_handle_t temp_dev_handle = NULL;
    ret = i2c_master_bus_add_device(bus_handle, &module_dev_cfg, &temp_dev_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create temporary device handle: %s", esp_err_to_name(ret));
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    if (write_request_id)
    {
        // Use transmit_receive: Write request_id first, then read
        ESP_LOGD(TAG, "Reading %d bytes from MUX %d Addr 0x%02X after writing Req 0x%02X",
                 buffer_len, mux_channel, module_address, request_id);
        ret = i2c_master_transmit_receive(temp_dev_handle,
                                          &request_id, 1,     // Write request ID
                                          buffer, buffer_len, // Read into buffer
                                          I2C_TIMEOUT_MS);
    }
    else
    {
        // Use receive only: Directly read from the device
        ESP_LOGD(TAG, "Reading %d bytes from MUX %d Addr 0x%02X (no write phase)",
                 buffer_len, mux_channel, module_address);
        ret = i2c_master_receive(temp_dev_handle,
                                 buffer, buffer_len,
                                 I2C_TIMEOUT_MS);
    }

    // Remove the temporary device
    i2c_master_bus_rm_device(temp_dev_handle);

    if (ret == ESP_OK)
    {
        // The new driver functions perform the full read operation as requested.
        // We assume the number of bytes read is buffer_len on success,
        // although the driver doesn't explicitly return this count here.
        // For protocols needing variable length reads, a different approach
        // (e.g., reading length first) would be needed.
        *bytes_read = buffer_len;
        ESP_LOGD(TAG, "Successfully read %d bytes from 0x%02X on MUX %d",
                 *bytes_read, module_address, mux_channel);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read data from 0x%02X on MUX %d: %s",
                 module_address, mux_channel, esp_err_to_name(ret));
        *bytes_read = 0;
    }

    xSemaphoreGive(i2c_mutex);
    return ret;
}

esp_err_t i2c_manager_probe_device(uint8_t device_address)
{
    // Default to using MUX channel 0 for simple probing
    uint8_t mux_channel = 0;
    esp_err_t ret;

    if (!bus_handle)
    {
        ESP_LOGE(TAG, "I2C Manager not initialized for probe");
        return ESP_ERR_INVALID_STATE;
    }

    // Mutex is needed to ensure MUX channel selection is stable during probe
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS * 2)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for probe");
        return ESP_ERR_TIMEOUT;
    }

    // Select the MUX channel
    ret = i2c_manager_select_mux_channel(mux_channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to select MUX channel %d before probing", mux_channel);
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    // NOTE: MUX channel must be selected *before* calling probe.
    i2c_device_config_t probe_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_address, // Using device_address instead of dev_addr
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    // Create a temporary device handle for this operation
    i2c_master_dev_handle_t temp_dev_handle = NULL;
    ret = i2c_master_bus_add_device(bus_handle, &probe_dev_cfg, &temp_dev_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create temporary device handle for probe: %s", esp_err_to_name(ret));
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    // Perform a zero-byte write transaction. Success (ESP_OK) indicates an ACK.
    // Timeout (ESP_ERR_TIMEOUT) indicates no ACK (NACK or bus busy/stuck).
    ret = i2c_master_transmit(temp_dev_handle, NULL, 0, pdMS_TO_TICKS(50)); // Short timeout for probe

    // Remove the temporary device
    i2c_master_bus_rm_device(temp_dev_handle);

    if (ret == ESP_OK)
    {
        ESP_LOGD(TAG, "Probe ACK received from address 0x%02X on MUX %d", device_address, mux_channel);
    }
    else if (ret == ESP_ERR_TIMEOUT)
    {
        ESP_LOGD(TAG, "Probe NACK/Timeout for address 0x%02X on MUX %d", device_address, mux_channel);
        // Map timeout specifically to ESP_FAIL or a custom error if needed,
        // but often ESP_ERR_TIMEOUT is descriptive enough for a failed probe.
    }
    else
    {
        ESP_LOGW(TAG, "Probe failed for address 0x%02X on MUX %d with error: %s",
                 device_address, mux_channel, esp_err_to_name(ret));
    }

    xSemaphoreGive(i2c_mutex);
    return (ret == ESP_OK) ? ESP_OK : ESP_ERR_NOT_FOUND; // Return ESP_OK only if ACK was received
}

esp_err_t i2c_manager_probe_device_on_channel(uint8_t mux_channel, uint8_t device_address)
{
    esp_err_t ret;

    if (!bus_handle)
    {
        ESP_LOGE(TAG, "I2C Manager not initialized for probe");
        return ESP_ERR_INVALID_STATE;
    }

    // Mutex is needed to ensure MUX channel selection is stable during probe
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS * 2)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for probe");
        return ESP_ERR_TIMEOUT;
    }

    // Select the MUX channel
    ret = i2c_manager_select_mux_channel(mux_channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to select MUX channel %d before probing", mux_channel);
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    // NOTE: MUX channel must be selected *before* calling probe.
    i2c_device_config_t probe_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_address, // Using device_address instead of dev_addr
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    // Create a temporary device handle for this operation
    i2c_master_dev_handle_t temp_dev_handle = NULL;
    ret = i2c_master_bus_add_device(bus_handle, &probe_dev_cfg, &temp_dev_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create temporary device handle for probe: %s", esp_err_to_name(ret));
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    // Perform a zero-byte write transaction. Success (ESP_OK) indicates an ACK.
    // Timeout (ESP_ERR_TIMEOUT) indicates no ACK (NACK or bus busy/stuck).
    ret = i2c_master_transmit(temp_dev_handle, NULL, 0, pdMS_TO_TICKS(50)); // Short timeout for probe

    // Remove the temporary device
    i2c_master_bus_rm_device(temp_dev_handle);

    if (ret == ESP_OK)
    {
        ESP_LOGD(TAG, "Probe ACK received from address 0x%02X on MUX %d", device_address, mux_channel);
    }
    else if (ret == ESP_ERR_TIMEOUT)
    {
        ESP_LOGD(TAG, "Probe NACK/Timeout for address 0x%02X on MUX %d", device_address, mux_channel);
        // Map timeout specifically to ESP_FAIL or a custom error if needed,
        // but often ESP_ERR_TIMEOUT is descriptive enough for a failed probe.
    }
    else
    {
        ESP_LOGW(TAG, "Probe failed for address 0x%02X on MUX %d with error: %s",
                 device_address, mux_channel, esp_err_to_name(ret));
    }

    xSemaphoreGive(i2c_mutex);
    return (ret == ESP_OK) ? ESP_OK : ESP_ERR_NOT_FOUND; // Return ESP_OK only if ACK was received
}