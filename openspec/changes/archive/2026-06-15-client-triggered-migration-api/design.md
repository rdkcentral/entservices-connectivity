# Design: Client-Triggered Migration API

## Architecture Overview

Migration state is tracked by the presence of a `fsChecksumAtLastSync` key in the RDK PersistentStore (`Bluetooth` namespace). This key doubles as:
1. A migration-state sentinel (`_isMigrated` is derived from its presence at `init()`)
2. The stored checksum used by `performMigration` to detect AS file changes on subsequent calls

```
RDK Persistent Store (Bluetooth namespace)
┌─────────────────────┬──────────────────────────────────────────┐
│ deviceInfo          │  [...array of device objects...]          │
├─────────────────────┼──────────────────────────────────────────┤
│ fsChecksumAtLastSync│  "<checksum string>"           (NEW)      │
└─────────────────────┴──────────────────────────────────────────┘

AS File Storage (filesystem)
┌──────────────────────────────────────────┐
│ paired_bluetooth_devices.json            │
│  (source of truth for migration)         │
└──────────────────────────────────────────┘
```

---

## init() Changes

Auto-migration is removed. `init()` instead:
1. Reads PS as today
2. Checks for `fsChecksumAtLastSync` key → sets `_isMigrated = true/false`
3. Reads from BTRMGR as today
4. Only calls `writeStorageFromCache()` if `_isMigrated == true`

---

## performMigration()

```
performMigration():
  if fsChecksumAtLastSync not in PS:     // first time
    read AS file → import to cache
    write cache to PS (deviceInfo)
    compute checksum of AS file content
    write checksum to PS (fsChecksumAtLastSync)
    _isMigrated = true
  else:                                  // subsequent calls
    compute checksum of current AS file
    read stored checksum from PS
    if same → return SUCCESS (no-op)
    if different:
      re-import from AS file → overwrite cache
      write cache to PS
      update fsChecksumAtLastSync in PS
```

## clearMigration()

```
clearMigration():
  delete deviceInfo key from PS
  delete fsChecksumAtLastSync key from PS
  clear _pairedDeviceCache
  _isMigrated = false
```

Note: `clearMigration` affects only RDK PersistentStore. BTRMGR pairing data is untouched.

---

## Pre-Migration Guard on setAutoConnect

When `_isMigrated == false`, `setAutoConnect()` returns an error (rejected). `getAutoConnect()` is unchanged — it performs a normal cache lookup regardless of migration state.

---

## writeStorageFromCache() Gate

`writeStorageFromCache()` skips the PS write when `_isMigrated == false`. The in-memory cache may still be updated; it simply is not persisted until migration has been performed. This is a single gate in one place rather than guarding every call site.

---

## Open Questions

### 1. performMigration re-sync semantics ("new entries")
The slide deck states `performMigration` should sync "only if AS Store has new entries." The checksum approach detects any change to the AS file, not just additions. On a detected change, the current design does a full overwrite of the cache (not a merge).

**Decision: Full overwrite.** When re-sync is triggered, the AS file is fully re-imported and overwrites RDK PS data. The AS file is the authoritative source.

### 2. Checksum algorithm stability
The existing `writeFilesystemPersistenceFromCache()` uses `std::hash<std::string>` for change detection, which is not guaranteed stable across process restarts or platforms. For `fsChecksumAtLastSync`, which is persisted to PS and compared across boots, `std::hash<std::string>` is not suitable.

**Decision: FNV-1a.** Self-contained, no external dependencies, deterministic and stable across boots and platforms. Implemented inline (~10 lines).

### 3. addDevice / removeDevice in pre-migration state
Pairing a new device while `_isMigrated == false` calls `addDevice()` → `writeStorageFromCache()`. With the `writeStorageFromCache()` gate in place, the cache is updated but PS is not written.

**Decision: Accept the silent cache-only update.** Pre-migration pairing is not an expected scenario in practice. `addDevice`/`removeDevice` are not explicitly rejected; the `writeStorageFromCache()` gate is sufficient.

### 4. clearMigration with active connections
If `clearMigration` is called while devices are connected, those devices remain connected at the BTRMGR level but their PS state is wiped. The AC status for connected devices would revert to `AUTO_CONNECT_STATUS_UNSET` in-cache.

**Decision: Caller is responsible.** `clearMigration` wipes PS and cache unconditionally. The client is expected to ensure clean state before calling it. No disconnect logic added to `clearMigration`.

### 5. performMigration concurrency
If `performMigration` is called concurrently (e.g., IUI race on first boot), two threads could both detect `fsChecksumAtLastSync` absent and both attempt import. The existing `_adminLock` covers cache writes but not the full migration sequence.

**Decision: Dedicated migration mutex.** A separate mutex wraps the entire `performMigration` sequence (read AS file, compute checksum, write PS) so only one thread executes it at a time. Prevents double-import and redundant PS writes.
