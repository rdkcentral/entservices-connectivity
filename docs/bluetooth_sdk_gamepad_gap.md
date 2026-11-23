# Gamepad logic in BTMgr that the SDK-based plugin does not have

This note lists the gamepad rules that BTMgr applies and that the plugin does not apply on the SDK backend, and who owns each one. The SDK team has reviewed the list: the SDK handles most of it, the plugin handles the Stadia autoconnect case only, and the RFC gate is not needed for now (see the summary at the end). See also [bluetooth_sdk_gap_report.md](./bluetooth_sdk_gap_report.md) and the known-differences table in [bluetooth_backend_qa_test_guide.md](./bluetooth_backend_qa_test_guide.md).

Sources: `btrMgr_ConnectionInAuthCb` and related code in `bluetooth_mgr/src/ifce/btrMgr.c`, `bluetooth/src/btrCore.c`, `bluetooth_mgr/include/btmgr.h`. The plugin side is `Bluetooth/AuthBridge.cpp` and `Bluetooth/AuthBridge.h`.

## 1. Unpaired devices are rejected outright

- BTMgr denies an incoming HID connection from a device that is not paired. This covers every HID device (gamepad, keyboard, mouse, remote), not only gamepads.
- The plugin sends the request to the client, which can accept it.
- **Owner: SDK** (agreed with the SDK team). Nothing to do in the plugin.

## 2. Hold-off after a disconnect (10 s)

- When a paired HID device disconnects, BTMgr remembers it as "last disconnected" for 10 s (`BTRMGR_DISCONNECT_CLEAR_STATUS_TIME_INTERVAL`).
- During that window it denies a reconnect from the same device if it is one of these models:

  | Model | Vendor | Product |
  |---|---|---|
  | Xbox Elite | 0x045E | 0x0B05 |
  | GameSir (Xbox mode) | 0x1949 | 0x0402 |
  | GameSir (Nintendo mode) | 0x057E | 0x2009 |
  | Xbox Adaptive | 0x045E | 0x0B0C |

- Each denial restarts the 10 s timer, unless gamepad standby mode is on.
- BTMgr's source comment says these gamepads reconnect by themselves almost as soon as they start to disconnect.
- The plugin has no hold-off.
- **Owner: SDK** (agreed with the SDK team). Nothing to do in the plugin.

## 3. Gamepad RFC gate

- BTMgr reads `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.BTR.GamePad.Enable` once at startup. It is forced on for Raspberry Pi builds.
- When it is off, BTMgr rejects incoming gamepad connections, hides gamepads from the discovered-devices list, and ignores status callbacks for gamepads.
- The plugin has no RFC check.
- **Not needed for now.** The SDK team confirmed the plugin does not need to do the RFC check.

## 4. Unsupported Xbox Gen4 gamepad filter

- Applies only in builds with `BT_UNSUPPORTED_GAMEPAD_ENABLED` (tied to Bluetooth 5.2 or later, or privacy).
- `BTRCore_IsUnsupportedGamepad` ignores Xbox Gen4 pads: vendor 0x045E, product 0x0B13, device ID 0x0509 or 0x0501.
- The plugin has no equivalent.
- **Owner: SDK** (agreed with the SDK team). Nothing to do in the plugin.

## 5. Other gamepad-specific BTMgr code (not analyzed)

- A delay before reporting out-of-range for the Xbox Elite (`gEliteIncomCon`): handled in the SDK (same as section 2).
- PS4 special case (`btrMgr_IsPS4Gamepad`): handled in the SDK.
- BTMgr unpairs an LE gamepad that pairs within 2 minutes of a reported pairing failure (`ghBTRMgrDevHdlLastPaired`): handled in the SDK.

## Already mirrored in the plugin

- Gamepad standby mode: reject gamepad connections in standby, remove and restore auto-connect, hold discovery events.
- Stadia special case, in the plugin as agreed with the SDK team: auto-connect is removed and restored (`setAutoconnectOff()` / `setAutoconnectOn()`) only for paired LE HID devices. A device is LE HID if its appearance is 0x03C4, its UUIDs contain 1812, or it is a Stadia controller (MAC starts with 61:47:AA, EF:6F:4D or CA:7B:25, or the name contains "Stadia"). RDK remotes and classic (non-zero class of device) devices are skipped. See `ScanFilterPolicy::isLeHidDevice`.
- Gamepads are never auto-accepted.
- Client answer windows of 2 s (classic gamepad) and 20 s (LE gamepad).

## Summary of decisions (agreed with the SDK team)

| Item | Owner |
|---|---|
| 1. Reject unpaired HID connections | SDK |
| 2. Hold-off after a disconnect (including `gEliteIncomCon`) | SDK |
| 3. Gamepad RFC gate | Not needed for now |
| 4. Unsupported Xbox Gen4 gamepad filter | SDK |
| 5. PS4 special case | SDK |
| 5. Last-paired LE gamepad unpair (`ghBTRMgrDevHdlLastPaired`) | SDK |
| 5. Stadia special case | Plugin (autoconnect handling, see "Already mirrored in the plugin") |
