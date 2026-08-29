#include "BtSdkAdapterImpl.h"

extern "C" WPEFramework::Plugin::IBtAdapter* CreateBluetoothSdkAdapter() {
    return new WPEFramework::Plugin::BtSdkAdapterImpl();
}

extern "C" void DestroyBluetoothSdkAdapter(WPEFramework::Plugin::IBtAdapter* adapter) {
    delete adapter;
}