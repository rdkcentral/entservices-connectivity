/**
 * NetworkCommissioning Bridge Header
 *
 * C API for initializing NetworkCommissioning WiFi driver
 * Safe to use across compilation boundaries
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize NetworkCommissioning WiFi driver for the given endpoint
 *
 * @param endpoint The endpoint ID to register NetworkCommissioning on (typically 0)
 * @return 0 on success, -1 on failure
 */
int barton_network_commissioning_init(uint16_t endpoint);

/**
 * Shutdown NetworkCommissioning
 */
void barton_network_commissioning_shutdown();

#ifdef __cplusplus
}
#endif
