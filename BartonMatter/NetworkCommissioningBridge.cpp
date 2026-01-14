/**
 * NetworkCommissioning Bridge
 *
 * This file provides a stub that delegates to BartonCore's NetworkCommissioning initialization.
 * The actual driver instantiation happens in BartonCore (compiled with Matter SDK flags),
 * not here in the plugin (to avoid ABI incompatibilities).
 */

#include "NetworkCommissioningBridge.h"

// Forward declare the BartonCore function
extern "C" int barton_matter_init_network_commissioning(uint16_t endpoint);

// C-style API for plugin use
extern "C" {

int barton_network_commissioning_init(uint16_t endpoint)
{
    // Delegate to BartonCore function which is compiled with Matter SDK flags
    return barton_matter_init_network_commissioning(endpoint);
}

void barton_network_commissioning_shutdown()
{
    // Nothing to do - BartonCore manages lifecycle
}

} // extern "C"

