# AC1 Validation Evidence Template

Use this file to capture raw outputs for tasks 5.1 through 5.4.

## Change

- Change: implement-ac1-migration-path
- Date run:
- Runner/environment:
- Branch/commit:

## Preconditions

- Required dependencies available (WPEFramework, CmakeHelperFunctions, toolchain paths).
- Any AS test file setup path access documented.

## 5.1 PersistentStore Present Scenario

### Goal

Confirm init uses Bluetooth PersistentStore data and does not execute AS import path.

### Command(s)

Paste exact commands used:

```sh
pwd
git rev-parse --short HEAD
cmake -S . -B build-ac1-on -DBLUETOOTH_ENABLE_AS_MIGRATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-ac1-on --target L1TestsBT
# Run your environment-specific L1 test launcher for Bluetooth tests here.
# Example placeholder (replace with your actual runner):
# ctest --test-dir build-ac1-on --output-on-failure
```

### Raw Output

```text
<paste raw terminal output here>
```

### Result

- Status: PASS / FAIL
- Evidence notes:

## 5.2 Store Missing + Valid AS Scenario

### Goal

Confirm init imports from AS and persists imported cache to Bluetooth PersistentStore.

### Command(s)

Paste exact commands used:

```sh
mkdir -p /opt/persistent/sky
cat > /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json <<'JSON'
{"pairedDevices":[{"deviceAddr":"123","friendlyName":"TVRemote","deviceType":"HEADPHONES","lastVolumeSetting":10,"autoConnectStatus":true,"lastConnectionTimeUTC":1712345678}]}
JSON

cmake -S . -B build-ac1-on -DBLUETOOTH_ENABLE_AS_MIGRATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-ac1-on --target L1TestsBT
# Run your environment-specific L1 test launcher for Bluetooth tests here.
# Example placeholder (replace with your actual runner):
# ctest --test-dir build-ac1-on --output-on-failure
```

### Raw Output

```text
<paste raw terminal output here>
```

### Result

- Status: PASS / FAIL
- Evidence notes:

## 5.3 Store Missing + Malformed or Missing AS Scenario

### Goal

Confirm init remains non-fatal, logs migration fallback, and continues reconciliation behavior.

### Command(s)

Paste exact commands used:

```sh
rm -f /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
cmake -S . -B build-ac1-on -DBLUETOOTH_ENABLE_AS_MIGRATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-ac1-on --target L1TestsBT
# Run your environment-specific L1 test launcher for Bluetooth tests here.
# Example placeholder (replace with your actual runner):
# ctest --test-dir build-ac1-on --output-on-failure

# Optional malformed-file variant:
mkdir -p /opt/persistent/sky
echo '{"pairedDevices": [' > /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
# Re-run the same test launcher and collect output.
```

### Raw Output

```text
<paste raw terminal output here>
```

### Result

- Status: PASS / FAIL
- Evidence notes:

## 5.4 CPESP-9452 Attachment Checklist

- [ ] 5.1 scenario output attached to CPESP-9452
- [ ] 5.2 scenario output attached to CPESP-9452
- [ ] 5.3 scenario output attached to CPESP-9452
- [ ] AC1 implementation summary attached

## Final Closure Summary

- 5.1 status: PASS / FAIL
- 5.2 status: PASS / FAIL
- 5.3 status: PASS / FAIL
- 5.4 evidence attached: YES / NO
- Follow-up actions:
