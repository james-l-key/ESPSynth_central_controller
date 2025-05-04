#pragma once

#include "module_i2c_proto.h" // Includes definitions like ParamId_t, ParamValue_t, ModuleType_t etc.
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>            // For size_t
#include "freertos/FreeRTOS.h" // For TickType_t

#ifdef __cplusplus
extern "C"
{
#endif

    // --- Configuration ---

    typedef struct
    {
        int i2c_port;                // I2C_NUM_0 or I2C_NUM_1
        int sda_io_num;              // GPIO number for SDA
        int scl_io_num;              // GPIO number for SCL
        uint32_t clk_speed;          // I2C clock speed (e.g., 100000 for 100kHz, 400000 for 400kHz)
        uint8_t tca9548a_addr;       // I2C address of the TCA9548A mux itself
        size_t task_stack_size;      // Stack size for the dedicated I2C manager task
        UBaseType_t task_priority;   // Priority for the I2C manager task
        int task_core_id;            // Core to pin the task to (0, 1, or tskNO_AFFINITY)
        uint32_t command_queue_size; // Max number of outstanding I2C requests
    } i2c_manager_config_t;

    // --- Discovered Module Info ---

    typedef struct
    {
        uint8_t mux_channel;      // Mux channel (0-7) where module was found
        uint8_t i2c_address;      // Slave address (0x08-0x77) of the module
        ModuleType_t module_type; // Type reported by the module
        uint16_t fw_version;      // Firmware version reported by the module
        uint8_t status;           // Last known status byte read from module
        // Add more fields as needed (e.g., unique ID if implemented)
        bool present; // Flag indicating if module responded during last scan
    } discovered_module_t;

    // --- Initialization / Deinitialization ---

    /**
     * @brief Initialize the I2C Manager component.
     * Configures I2C master driver, tests communication with mux.
     * Creates the command queue and starts the dedicated I2C manager task.
     *
     * @param config Pointer to configuration struct.
     * @return ESP_OK on success, or an error code.
     */
    esp_err_t i2c_manager_init(const i2c_manager_config_t *config);

    /**
     * @brief Deinitialize the I2C Manager component.
     * Stops the task, deletes the queue, uninstalls the I2C driver.
     *
     * @return ESP_OK on success.
     */
    esp_err_t i2c_manager_deinit(void);

    // --- Asynchronous Write/Command Functions (Queue-based) ---
    // These functions queue a request and return quickly. The actual I2C operation
    // happens later in the dedicated task. They return ESP_OK if successfully queued.

    /**
     * @brief Queue a request to set a parameter on a specific module.
     *
     * @param mux_channel The mux channel (0-7) the module is on.
     * @param module_addr The I2C slave address of the module.
     * @param param_id The parameter to set (ParamId_t).
     * @param value The value to set the parameter to (ParamValue_t).
     * @return ESP_OK if the request was successfully queued, ESP_FAIL or ESP_ERR_TIMEOUT if queue is full.
     */
    esp_err_t i2c_manager_queue_set_param(uint8_t mux_channel, uint8_t module_addr, ParamId_t param_id, ParamValue_t value);

    /**
     * @brief Queue a request to configure I2S slots for a specific module.
     *
     * @param mux_channel The mux channel (0-7).
     * @param module_addr The I2C slave address.
     * @param config The I2S configuration data.
     * @return ESP_OK if the request was successfully queued, ESP_FAIL or ESP_ERR_TIMEOUT if queue is full.
     */
    esp_err_t i2c_manager_queue_set_i2s_config(uint8_t mux_channel, uint8_t module_addr, const I2sConfig_t config); // Pass config by value

    /**
     * @brief Queue a request to send a simple command (no payload) to a module.
     *
     * @param mux_channel The mux channel (0-7).
     * @param module_addr The I2C slave address.
     * @param command The command byte (e.g., CMD_COMMON_RESET).
     * @return ESP_OK if the request was successfully queued, ESP_FAIL or ESP_ERR_TIMEOUT if queue is full.
     */
    esp_err_t i2c_manager_queue_send_command(uint8_t mux_channel, uint8_t module_addr, uint8_t command);

    // --- Synchronous Read Functions (Blocking) ---
    // These functions perform the I2C read operation directly (within the caller's context,
    // but internally they might signal the I2C task or use a mutex for bus access).
    // Simpler approach for reads: Use a mutex internally for these blocking functions.

    /**
     * @brief Read a common register from a specific module (Blocking).
     * Handles mux switching, write-read sequence, and uses internal mutex for bus access.
     *
     * @param mux_channel The mux channel (0-7).
     * @param module_addr The I2C slave address.
     * @param reg_addr The common register address to read (CommonReadRegAddr_t).
     * @param[out] buffer Pointer to store the read data.
     * @param read_size Expected number of bytes to read for this register.
     * @param timeout_ms Max time to wait for the transaction (including mutex acquisition).
     * @return ESP_OK on success, or error code (ESP_FAIL, ESP_ERR_TIMEOUT, ESP_ERR_INVALID_ARG).
     */
    esp_err_t i2c_manager_read_common_reg(uint8_t mux_channel, uint8_t module_addr, uint8_t reg_addr, uint8_t *buffer, size_t read_size, TickType_t timeout_ticks);

    // --- Convenience Read Functions (Wrappers around i2c_manager_read_common_reg) ---

    /**
     * @brief Get the module type (Blocking).
     *
     * @param mux_channel Mux channel.
     * @param module_addr Slave address.
     * @param[out] module_type Pointer to store the ModuleType_t.
     * @param timeout_ticks Timeout in FreeRTOS ticks.
     * @return ESP_OK on success.
     */
    esp_err_t i2c_manager_get_module_type(uint8_t mux_channel, uint8_t module_addr, ModuleType_t *module_type, TickType_t timeout_ticks);

    /**
     * @brief Get the module status byte (Blocking).
     *
     * @param mux_channel Mux channel.
     * @param module_addr Slave address.
     * @param[out] status Pointer to store the status byte.
     * @param timeout_ticks Timeout in FreeRTOS ticks.
     * @return ESP_OK on success.
     */
    esp_err_t i2c_manager_get_status(uint8_t mux_channel, uint8_t module_addr, uint8_t *status, TickType_t timeout_ticks);

    /**
     * @brief Get the module firmware version (Blocking).
     *
     * @param mux_channel Mux channel.
     * @param module_addr Slave address.
     * @param[out] version Pointer to store the uint16_t version.
     * @param timeout_ticks Timeout in FreeRTOS ticks.
     * @return ESP_OK on success.
     */
    esp_err_t i2c_manager_get_fw_version(uint8_t mux_channel, uint8_t module_addr, uint16_t *version, TickType_t timeout_ticks);

    // --- Discovery ---

    /**
     * @brief Scans the I2C bus (all mux channels, specified addresses) for modules (Blocking).
     * This operation takes time. It uses the internal mutex for bus access during scans.
     *
     * @param[out] found_modules_buffer Buffer to store details of found modules.
     * @param buffer_capacity Max number of modules the buffer can hold.
     * @param[out] count Pointer to store the actual number of modules found and populated in the buffer.
     * @param addresses_to_scan Array of potential slave addresses to check (e.g., {0x20, 0x21, 0x22...}). Set to NULL to scan default range (0x08-0x77).
     * @param num_addresses Number of addresses in addresses_to_scan array (ignored if addresses_to_scan is NULL).
     * @param timeout_ms_per_device Timeout for pinging/querying each potential device address on each channel.
     * @return ESP_OK on successful scan completion (even if no modules found).
     */
    esp_err_t i2c_manager_discover_modules(discovered_module_t *found_modules_buffer,
                                           size_t buffer_capacity,
                                           size_t *count,
                                           const uint8_t *addresses_to_scan, // Optional: NULL to scan default range
                                           size_t num_addresses,             // Ignored if addresses_to_scan is NULL
                                           uint32_t timeout_ms_per_device);

#ifdef __cplusplus
}
#endif