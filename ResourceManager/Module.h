#ifndef MODULE_NAME
#define MODULE_NAME Plugin_TOP
#endif

#include <plugins/plugins.h>
#include <tracing/tracing.h>

#undef EXTERNAL
#define EXTERNAL

// MODULE_NAME is how Thunder identifies which plugin a log/trace message came from. Every plugin must have a unique one.
// It's the single include hub — your ResourceManager.h does #include "Module.h" and gets everything Thunder provides through it. You don't include plugins/plugins.h directly anywhere else.
// EXTERNAL is a Thunder macro for controlling symbol visibility in the shared library.