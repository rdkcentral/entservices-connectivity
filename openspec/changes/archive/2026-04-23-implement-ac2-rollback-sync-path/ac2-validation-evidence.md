# AC2 Validation Evidence Template

Use this file to capture raw outputs for tasks 5.1 through 5.3.

## Change

- Change: implement-ac2-rollback-sync-path
- Date run:
- Runner/environment:
- Branch/commit:

## Preconditions

- Required dependencies available (WPEFramework, CmakeHelperFunctions, toolchain paths).
- BLUETOOTH_ENABLE_AS_MIGRATION ON/OFF build toggles available.

## Telemetry Marker Documentation (Task 3.3)

Document marker presence or equivalent diagnostic coverage used for AC2 rollback sync:

- rollback_sync_success marker name used:
- rollback_sync_failure marker name used:
- Emission location(s):
- If markers are not compiled in this environment, document fallback structured logs used for AC2 diagnostics:

## 5.1 ON-Mode Validation (Sync Expected)

### Goal

Confirm mutating writes mirror to AS file when BLUETOOTH_ENABLE_AS_MIGRATION is ON.

### Command(s)

```sh
cmake -S . -B build-ac2-on -DBLUETOOTH_ENABLE_AS_MIGRATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-ac2-on --target L1TestsBT
# Run your environment-specific Bluetooth L1 runner here
# Example placeholder:
# ctest --test-dir build-ac2-on --output-on-failure
```

### Raw Output (ON)

```text
<paste raw terminal output here>
```

### Result (ON)

- Status: PASS / FAIL
- Notes:

## 5.2 OFF-Mode Validation (No Sync Expected)

### Goal

Confirm mutating writes do not mirror to AS file when BLUETOOTH_ENABLE_AS_MIGRATION is OFF.

### Command(s)

```sh
cmake -S . -B build-ac2-off -DBLUETOOTH_ENABLE_AS_MIGRATION=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-ac2-off --target L1TestsBT
# Run your environment-specific Bluetooth L1 runner here
# Example placeholder:
# ctest --test-dir build-ac2-off --output-on-failure
```

### Raw Output (OFF)

```text
<paste raw terminal output here>
```

### Result (OFF)

- Status: PASS / FAIL
- Notes:

## 5.3 CPESP-9452 Attachment Checklist

- [ ] ON-mode command output attached
- [ ] OFF-mode command output attached
- [ ] AC2 sync behavior summary attached
- [ ] Rollback sync diagnostics evidence attached

## Final Summary

- 5.1 status: PASS / FAIL
- 5.2 status: PASS / FAIL
- 5.3 evidence attached: YES / NO
- Follow-up actions:
