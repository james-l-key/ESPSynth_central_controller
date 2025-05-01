#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h> // For size_t
#include <stdbool.h>

/**
 * @brief Initialize the I2C Master driver and TCA9548A multiplexer.
 *
 * Reads configuration from sdkconfig for pins, port, frequency, and mux address.
 * Uses the new ESP-IDF I2C master driver API.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t i2c_manager_init(void);

/**
 * @brief Deinitialize the I2C Manager, releasing resources.
 *
 * @return ESP_OK on success.
 */
esp_err_t i2c_manager_deinit(void);

/**
 * @brief Select the active downstream channel on the TCA9548A I2C multiplexer.
 *
 * @param channel The channel number to select (0-7).
 * @return ESP_OK on success, or an error code if communication with the mux fails.
 */
esp_err_t i2c_manager_select_mux_channel(uint8_t channel);

/**
 * @brief Send a command (write operation) to a module on a specific MUX channel.
 *
 * This function handles MUX channel selection and the I2C write transaction.
 * Assumes a protocol where the command_id is sent first, followed by data.
 *
 * @param mux_channel The I2C mux channel the module is on.
 * @param module_address The 7-bit I2C address of the module.
 * @param command_id The command identifier (defined in module_i2c_proto.h).
 * @param data Pointer to the command data payload (can be NULL if data_len is 0).
 * @param data_len Length of the command data payload.
 * @return ESP_OK on success, ESP_FAIL or other error code on failure.
 */
esp_err_t i2c_manager_send_command(uint8_t mux_channel, uint8_t module_address, uint8_t command_id, const void *data, size_t data_len);

/**
 * @brief Read data or status from a module on a specific MUX channel.
 *
 * This function handles MUX channel selection and the I2C read transaction.
 * Assumes a protocol where a request_id might be written first (optional,
 * depending on protocol), followed by a read operation.
 *
 * @param mux_channel The I2C mux channel the module is on.
 * @param module_address The 7-bit I2C address of the module.
 * @param request_id Identifier for the data/status being requested (can be 0 if no write phase needed).
 * @param write_request_id If true, the request_id byte will be written before the read phase.
 * @param buffer Pointer to the buffer where read data will be stored.
 * @param buffer_len Size of the read buffer (max bytes to read).
 * @param[out] bytes_read Pointer to store the actual number of bytes read.
 * @return ESP_OK on success, ESP_FAIL or other error code on failure.
 */
esp_err_t i2c_manager_read_data(uint8_t mux_channel, uint8_t module_address, uint8_t request_id, bool write_request_id, void *buffer, size_t buffer_len, size_t *bytes_read);

/**
 * @brief Probe for a device at a specific address on the currently selected MUX channel.
 *
 * Sends only the device address with the write bit and checks for an ACK.
 *
 * @param device_address The 7-bit I2C address to check.
 * @return ESP_OK if an ACK is received (device present), ESP_ERR_TIMEOUT if no ACK, or other error code.
 */
esp_err_t i2c_manager_probe_device(uint8_t device_address);

/**
 * @brief Probe a specific I2C device address on a specific MUX channel
 *
 * @param mux_channel MUX channel to select before probing (0-7)
 * @param device_address I2C device address to probe
 * @return ESP_OK if device responds with ACK, otherwise error
 */
esp_err_t i2c_manager_probe_device_on_channel(uint8_t mux_channel, uint8_t device_address);