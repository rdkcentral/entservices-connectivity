## 1. Runtime backend selection model

- [ ] 1.1 Define the runtime selection helper for checking `/usr/lib/bluetoothsdk/librdk_bluetooth.so`
- [ ] 1.2 Ensure the selected backend is cached once and not re-evaluated during the plugin lifetime
- [ ] 1.3 Preserve the current `BtAdapter::setImpl()` mock override behavior in test builds

## 2. Adapter wiring

- [ ] 2.1 Update `BtAdapter` production initialization to select the active backend instance at runtime
- [ ] 2.2 Ensure both SDK and BTMgr implementations are available via the adapter factory without changing call sites
- [ ] 2.3 Keep `Bluetooth.cpp` and the public plugin API behavior unchanged while routing through the selected instance

## 3. Build configuration

- [ ] 3.1 Update `Bluetooth/CMakeLists.txt` to compile both backends for the production binary
- [ ] 3.2 Keep L1/L2 test backend override logic intact through `BLUETOOTH_TEST_BACKEND`
- [ ] 3.3 Confirm include and link configuration remains correct for the selected backend path

## 4. Validation

- [ ] 4.1 Validate plugin initialization chooses the SDK implementation when the SDK library exists
- [ ] 4.2 Validate plugin initialization chooses the BTMgr implementation when the SDK library is absent
- [ ] 4.3 Run the minimal relevant build or test command for the Bluetooth plugin
