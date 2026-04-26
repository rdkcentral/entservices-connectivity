## Why

CPESP-9452 requires migration and rollback logic to be removable without risking baseline Bluetooth behavior. A single compile-time gate enabled by default is needed now so AC1 and AC2 code can be isolated, validated in both modes, and later removed safely.

## What Changes

- Add a unified compile-time option for CPESP-9452 migration and rollback paths, enabled by default.
- Export a single preprocessor define from Bluetooth build configuration and use it as the only ticket-level gate.
- Define build expectations for both modes:
  - Flag ON: migration and rollback ticket paths are compiled.
  - Flag OFF: AC1 and AC2 ticket paths are excluded while baseline Bluetooth persistence remains intact.
- Document precedence and optional runtime companion behavior if platform integration needs it.

## Capabilities

### New Capabilities
- bluetooth-feature-gating: Compile-time isolation for CPESP-9452 AC1 and AC2 ticket code with default-enabled behavior and clean OFF builds.

### Modified Capabilities
- None.

## Impact

- Affected build files: Bluetooth/CMakeLists.txt and potentially Bluetooth/Bluetooth.config.
- Affected implementation files: Bluetooth/BluetoothDeviceManager.h and Bluetooth/BluetoothDeviceManager.cpp where ticket code is conditionally compiled.
- Affected validation: L1 coverage must verify builds and behavior in both flag states.
- No external API contract change is introduced by this phase.

## Resolved Questions

- Platform build wrappers do not require additional option-forwarding work beyond Bluetooth/CMakeLists.txt for this change.
- CI matrix expansion with a dedicated OFF target is not included in this change scope.
