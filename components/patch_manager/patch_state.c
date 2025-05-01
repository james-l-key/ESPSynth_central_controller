#include "patch_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" // For mutex
#include "string.h"          // For memset
#include "synth_constants.h" // From common_definitions
#include "i2c_manager.h"     // To send routing commands

static const char *TAG = "PATCH_MANAGER";

// --- State ---
static patch_connection_t patch_connections[MAX_PATCH_CONNECTIONS];
static size_t active_connection_count = 0;
static SemaphoreHandle_t patch_mutex = NULL;

// --- Initialization ---

esp_err_t patch_manager_init(void)
{
    patch_mutex = xSemaphoreCreateMutex();
    if (patch_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create patch mutex");
        return ESP_FAIL;
    }

    // Clear the patch state
    memset(patch_connections, 0, sizeof(patch_connections));
    for (int i = 0; i < MAX_PATCH_CONNECTIONS; ++i)
    {
        patch_connections[i].is_active = false;
    }
    active_connection_count = 0;

    ESP_LOGI(TAG, "Patch Manager Initialized (Max Connections: %d)", MAX_PATCH_CONNECTIONS);
    return ESP_OK;
}

// --- Connection Management Stubs ---

esp_err_t patch_manager_add_connection(module_id_t source_module_id, port_id_t source_port_id, module_id_t dest_module_id, port_id_t dest_port_id)
{
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(patch_mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire patch mutex for add");
        return ESP_ERR_TIMEOUT;
    }

    // --- STUB ---
    ESP_LOGW(TAG, "STUB: %s called: %d:%d -> %d:%d", __func__,
             source_module_id, source_port_id, dest_module_id, dest_port_id);

    // 1. Find an empty slot
    int empty_slot = -1;
    for (int i = 0; i < MAX_PATCH_CONNECTIONS; ++i)
    {
        if (!patch_connections[i].is_active)
        {
            empty_slot = i;
            break;
        }
    }

    if (empty_slot == -1)
    {
        ESP_LOGE(TAG, "Cannot add connection: Patch matrix full (%d connections)", MAX_PATCH_CONNECTIONS);
        ret = ESP_ERR_NO_MEM;
    }
    else
    {
        // 2. Validate connection (Placeholder - needs module discovery info)
        ESP_LOGI(TAG, "Connection validation (placeholder)... OK");

        // 3. Determine TDM slots (Placeholder - needs complex logic)
        uint8_t tdm_slot = (uint8_t)active_connection_count; // Extremely basic placeholder
        ESP_LOGI(TAG, "Assigning TDM slot (placeholder): %d", tdm_slot);

        // 4. Send I2C commands (Placeholder - needs protocol definition & module info)
        ESP_LOGI(TAG, "Sending I2C routing commands (placeholder)...");
        // Example (pseudo-code):
        // module_info_t* src_info = get_module_info(source_module_id);
        // module_info_t* dst_info = get_module_info(dest_module_id);
        // if (src_info && dst_info) {
        //    tdm_config_payload_t src_payload = { .port = source_port_id, .slot = tdm_slot, .enable = true };
        //    ret = i2c_manager_send_command(src_info->mux_channel, src_info->i2c_addr, CMD_CONFIG_I2S_OUTPUT, &src_payload, sizeof(src_payload));
        //    if (ret == ESP_OK) {
        //       tdm_config_payload_t dst_payload = { .port = dest_port_id, .slot = tdm_slot, .enable = true };
        //       ret = i2c_manager_send_command(dst_info->mux_channel, dst_info->i2c_addr, CMD_CONFIG_I2S_INPUT, &dst_payload, sizeof(dst_payload));
        //    }
        // } else { ret = ESP_ERR_NOT_FOUND; }

        // Assuming I2C commands were successful (in this stub)
        ret = ESP_OK; // Placeholder success

        if (ret == ESP_OK)
        {
            // 5. Store the connection state
            patch_connections[empty_slot].source_module = source_module_id;
            patch_connections[empty_slot].source_port = source_port_id;
            patch_connections[empty_slot].dest_module = dest_module_id;
            patch_connections[empty_slot].dest_port = dest_port_id;
            patch_connections[empty_slot].is_active = true;
            active_connection_count++;
            ESP_LOGI(TAG, "Connection added successfully. Total active: %d", active_connection_count);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to configure routing for new connection via I2C.");
            // Rollback any partial configuration if necessary
        }
    }
    // --- END STUB ---

    xSemaphoreGive(patch_mutex);
    return ret;
}

esp_err_t patch_manager_remove_connection(module_id_t source_module_id, port_id_t source_port_id, module_id_t dest_module_id, port_id_t dest_port_id)
{
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(patch_mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire patch mutex for remove");
        return ESP_ERR_TIMEOUT;
    }

    // --- STUB ---
    ESP_LOGW(TAG, "STUB: %s called: %d:%d -> %d:%d", __func__,
             source_module_id, source_port_id, dest_module_id, dest_port_id);

    // 1. Find the connection
    int found_slot = -1;
    for (int i = 0; i < MAX_PATCH_CONNECTIONS; ++i)
    {
        if (patch_connections[i].is_active &&
            patch_connections[i].source_module == source_module_id &&
            patch_connections[i].source_port == source_port_id &&
            patch_connections[i].dest_module == dest_module_id &&
            patch_connections[i].dest_port == dest_port_id)
        {
            found_slot = i;
            break;
        }
    }

    if (found_slot == -1)
    {
        ESP_LOGW(TAG, "Connection to remove not found.");
        ret = ESP_ERR_NOT_FOUND;
    }
    else
    {
        // 2. Send I2C commands to deconfigure routing (Placeholder)
        ESP_LOGI(TAG, "Sending I2C de-routing commands (placeholder)...");
        // Example (pseudo-code):
        // module_info_t* src_info = get_module_info(source_module_id);
        // module_info_t* dst_info = get_module_info(dest_module_id);
        // if (src_info && dst_info) {
        //    tdm_config_payload_t src_payload = { .port = source_port_id, .slot = GET_SLOT_FOR_CONN(found_slot), .enable = false };
        //    ret = i2c_manager_send_command(src_info->mux_channel, src_info->i2c_addr, CMD_CONFIG_I2S_OUTPUT, &src_payload, sizeof(src_payload));
        //    // Don't necessarily need to disable input on dest, depends on module behavior
        // } else { ret = ESP_ERR_NOT_FOUND; }
        ret = ESP_OK; // Placeholder success

        if (ret == ESP_OK)
        {
            // 3. Update state
            patch_connections[found_slot].is_active = false;
            active_connection_count--;
            ESP_LOGI(TAG, "Connection removed successfully. Total active: %d", active_connection_count);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to deconfigure routing for removed connection via I2C.");
            // Should we still mark as inactive locally? Depends on desired robustness.
        }
    }
    // --- END STUB ---

    xSemaphoreGive(patch_mutex);
    return ret;
}

esp_err_t patch_manager_get_connections(patch_connection_t *connections_buffer, size_t buffer_size, size_t *count)
{
    if (connections_buffer == NULL || count == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(patch_mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to acquire patch mutex for get");
        return ESP_ERR_TIMEOUT;
    }

    // --- STUB ---
    ESP_LOGD(TAG, "STUB: %s called", __func__);

    size_t num_to_copy = 0;
    for (int i = 0; i < MAX_PATCH_CONNECTIONS && num_to_copy < buffer_size; ++i)
    {
        if (patch_connections[i].is_active)
        {
            memcpy(&connections_buffer[num_to_copy], &patch_connections[i], sizeof(patch_connection_t));
            num_to_copy++;
        }
    }
    *count = num_to_copy;
    // --- END STUB ---

    xSemaphoreGive(patch_mutex);
    return ESP_OK;
}
