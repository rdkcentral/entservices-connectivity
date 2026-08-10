#include "Module.h"

MODULE_NAME_DECLARATION(BUILD_REFERENCE)

// Module.h                          Module.cpp
//    |                                  |
//    ├── defines MODULE_NAME            ├── #include "Module.h"
//    ├── pulls in all Thunder headers   └── MODULE_NAME_DECLARATION(BUILD_REFERENCE)
//    └── included by ResourceManager.h      ↓
//                                     compiled into the .so
//                                     Thunder reads it at load time


