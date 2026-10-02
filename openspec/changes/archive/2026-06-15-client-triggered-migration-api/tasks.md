## 1. BluetoothDeviceManager — Header

- [x] 1.1 Add `PERSISTENT_STORE_KEY_FS_CHECKSUM` constant (`"fsChecksumAtLastSync"`) alongside existing PS key constants.
- [x] 1.2 Declare `performMigration()` as a public method (guarded by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).
- [x] 1.3 Declare `clearMigration()` as a public method (guarded by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).
- [x] 1.4 Add `bool _isMigrated` private member (guarded by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).
- [x] 1.5 Add `Core::CriticalSection _migrationLock` private member (guarded by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).
- [x] 1.6 Declare private helper `computeFNV1aChecksum(const std::string& content)` returning `std::string` (guarded by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).
- [x] 1.7 Declare private helper `readFsChecksumFromStorage(std::string& checksum)` returning `Core::hresult` (guarded by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).
- [x] 1.8 Declare private helper `writeFsChecksumToStorage(const std::string& checksum)` returning `Core::hresult` (guarded by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).

## 2. BluetoothDeviceManager — init()

- [x] 2.1 Remove the existing auto-migration block inside `init()` (the `if (missingFromPersistentStore(storageResult))` block under `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).
- [x] 2.2 After `updateCacheFromStorage()`, set `_isMigrated` by checking whether `fsChecksumAtLastSync` is present in PS (call `readFsChecksumFromStorage`; if it returns `ERROR_NONE`, `_isMigrated = true`, else `false`).
- [x] 2.3 Gate the final `writeStorageFromCache()` call in `init()` so it is skipped when `_isMigrated == false`.

## 3. BluetoothDeviceManager — setAutoConnect() Pre-Migration Guard

- [x] 3.1 At the top of `setAutoConnect()`, add a pre-migration guard (under `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`): if `_isMigrated == false`, log a warning and return an appropriate error code.

## 4. BluetoothDeviceManager — writeStorageFromCache() Gate

- [x] 4.1 At the top of `writeStorageFromCache()`, add a gate (under `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`): if `_isMigrated == false`, return `Core::ERROR_NONE` without writing to PS.

## 5. BluetoothDeviceManager — FNV-1a Checksum Helper

- [x] 5.1 Implement `computeFNV1aChecksum()` using FNV-1a 64-bit algorithm. Input is the raw AS file content string; output is the hash as a decimal string.

## 6. BluetoothDeviceManager — PS Checksum Helpers

- [x] 6.1 Implement `readFsChecksumFromStorage()`: reads `PERSISTENT_STORE_KEY_FS_CHECKSUM` from PS via `IStore::GetValue`. Returns `ERROR_NONE` + value on success, `ERROR_NOT_EXIST` / `ERROR_UNKNOWN_KEY` when absent.
- [x] 6.2 Implement `writeFsChecksumToStorage()`: writes `PERSISTENT_STORE_KEY_FS_CHECKSUM` to PS via `IStore::SetValue`.

## 7. BluetoothDeviceManager — performMigration()

- [x] 7.1 Acquire `_migrationLock` for the entire method body.
- [x] 7.2 Call `readFsChecksumFromStorage()` to determine first-time vs. subsequent call.
- [x] 7.3 Read the AS file content via `BluetoothPersistenceAdapter::Read()`.
- [x] 7.4 Compute FNV-1a checksum of the raw AS file content.
- [x] 7.5 On first time (checksum absent in PS): import devices via `writeCacheFromFilesystemPersistence()`, write cache to PS via `writeStorageFromCache()` (temporarily set `_isMigrated = true` before calling to bypass the gate), then write checksum to PS via `writeFsChecksumToStorage()`.
- [x] 7.6 On subsequent call (checksum present): compare new checksum with stored; if same, return `ERROR_NONE` (no-op); if different, re-import via `writeCacheFromFilesystemPersistence()`, overwrite PS via `writeStorageFromCache()`, update checksum via `writeFsChecksumToStorage()`.
- [x] 7.7 Ensure `_isMigrated = true` is set on any successful migration/re-sync path before releasing `_migrationLock`.

## 8. BluetoothDeviceManager — clearMigration()

- [x] 8.1 Acquire `_migrationLock` for the entire method body.
- [x] 8.2 Delete `PERSISTENT_STORE_KEY_DEVICE_INFO` from PS via `IStore` (if present).
- [x] 8.3 Delete `PERSISTENT_STORE_KEY_FS_CHECKSUM` from PS via `IStore` (if present).
- [x] 8.4 Clear `_pairedDeviceCache` under `_adminLock`.
- [x] 8.5 Set `_isMigrated = false`.

## 9. Bluetooth.h — New API Declarations

- [x] 9.1 Declare `METHOD_PERFORM_MIGRATION` and `METHOD_CLEAR_MIGRATION` static string constants.
- [x] 9.2 Declare `performMigrationWrapper` and `clearMigrationWrapper` public method wrappers (guarded by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).

## 10. Bluetooth.cpp — New API Registration and Implementation

- [x] 10.1 Define `METHOD_PERFORM_MIGRATION = "performMigration"` and `METHOD_CLEAR_MIGRATION = "clearMigration"` string constants.
- [x] 10.2 Register both methods in `Initialize()` (under `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`).
- [x] 10.3 Implement `performMigrationWrapper`: call `m_bluetoothDeviceManager.performMigration()`, return success/failure via `returnResponse`.
- [x] 10.4 Implement `clearMigrationWrapper`: call `m_bluetoothDeviceManager.clearMigration()`, return success/failure via `returnResponse`.
