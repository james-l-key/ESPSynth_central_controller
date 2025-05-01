#pragma once

// Define common constants and potentially simple enums/structs
// used across multiple components within the Central Controller firmware.

#define MAX_I2C_MUX_CHANNELS 8

// Example: Placeholder for maximum patch connections
#define MAX_PATCH_CONNECTIONS 64

// Timeout for I2C operations (milliseconds)
#define I2C_TIMEOUT_MS 1000 // Increased timeout for potentially slower modules

// Add more constants as needed (e.g., max modules, default sample rate)