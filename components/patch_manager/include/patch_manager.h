#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h> // For bool type
#include <stddef.h>  // For size_t

// Define basic types for identifying modules and their ports (inputs/outputs)
// These might become more complex later.
typedef uint16_t module_id_t; // Unique ID assigned to each discovered module instance
typedef uint8_t port_id_t;    // Identifier for an input or output port on a module

// Structure to represent a single connection in the patch matrix
typedef struct
{
    module_id_t source_module;
    port_id_t source_port;
    module_id_t dest_module;
    port_id_t dest_port;
    // Add other potential attributes like connection type (audio, cv), status etc.
    bool is_active;
} patch_connection_t;

/**
 * @brief Initialize the Patch Manager.
 *
 * Allocates resources for storing patch state.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t patch_manager_init(void);

/**
 * @brief Add a new connection to the patch matrix.
 *
 * NOTE: This is a stub. Implementation needs to:
 * 1. Validate the connection (e.g., module/port existence, type compatibility).
 * 2. Store the connection state.
 * 3. Determine the required I2S TDM slot configuration.
 * 4. Send I2C commands via i2c_manager to the relevant routing/processing modules
 * to configure their I2S peripherals for the new connection.
 *
 * @param source_module_id ID of the source module.
 * @param source_port_id ID of the source port on the source module.
 * @param dest_module_id ID of the destination module.
 * @param dest_port_id ID of the destination port on the destination module.
 * @return ESP_OK on success, ESP_FAIL or other error code on failure.
 */
esp_err_t patch_manager_add_connection(module_id_t source_module_id, port_id_t source_port_id, module_id_t dest_module_id, port_id_t dest_port_id);

/**
 * @brief Remove an existing connection from the patch matrix.
 *
 * NOTE: This is a stub. Implementation needs to:
 * 1. Find the connection to remove.
 * 2. Update the stored patch state.
 * 3. Send I2C commands via i2c_manager to the relevant modules to deconfigure
 * or reroute the associated I2S TDM slots.
 *
 * @param source_module_id ID of the source module of the connection to remove.
 * @param source_port_id ID of the source port of the connection to remove.
 * @param dest_module_id ID of the destination module of the connection to remove.
 * @param dest_port_id ID of the destination port of the connection to remove.
 * @return ESP_OK on success, ESP_FAIL if the connection doesn't exist or on error.
 */
esp_err_t patch_manager_remove_connection(module_id_t source_module_id, port_id_t source_port_id, module_id_t dest_module_id, port_id_t dest_port_id);

/**
 * @brief Get the current list of active patch connections.
 *
 * NOTE: This is a stub. Implementation needs to provide access to the stored state.
 *
 * @param[out] connections_buffer Pointer to an array to store the connections.
 * @param buffer_size The maximum number of connections the buffer can hold.
 * @param[out] count Pointer to store the actual number of active connections written to the buffer.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t patch_manager_get_connections(patch_connection_t *connections_buffer, size_t buffer_size, size_t *count);