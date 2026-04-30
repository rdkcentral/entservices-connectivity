# Validation Evidence Template

Use this file to capture raw command outputs for tasks 6.1, 6.2, and 6.3.

## Change

- Change: add-as-persistence-adapter
- Date run:
- Runner/environment:
- Branch/commit:

## Preconditions

- Required dependencies available (WPEFramework, CmakeHelperFunctions, toolchain paths).
- Build workspace cleaned or known state documented.

## 6.1 ON-Mode Validation

### Command(s)

Paste exact commands used:

```sh
cmake -S . -B build-as-on -DBLUETOOTH_ENABLE_AS_MIGRATION=ON
cmake --build build-as-on --target L1TestsBT
# add exact L1 execution command used in your environment
```

### Raw Output (ON)

Paste unedited output:

```text
<paste raw terminal output here>
```

### Result (ON)

- Status: PASS / FAIL
- Notes:

## 6.2 OFF-Mode Validation

### Command(s)

Paste exact commands used:

```sh
cmake -S . -B build-as-off -DBLUETOOTH_ENABLE_AS_MIGRATION=OFF
cmake --build build-as-off --target L1TestsBT
# add exact L1 execution command used in your environment
```

### Raw Output (OFF)

Paste unedited output:

```text
<paste raw terminal output here>
```

### Result (OFF)

- Status: PASS / FAIL
- Notes:

## Focus Checks (Optional but Recommended)

- Verify parse-fallback tests are executed:
  - asMigrationParseFallback_NumericTimestampAndStringBoolean
  - asMigrationParseFallback_StringTimestampAndNumericBoolean
- Verify feature-gate compile coverage test path for current flag mode.

## 6.3 CPESP-9452 Evidence Attachment Checklist

- [ ] ON-mode commands + raw output attached to CPESP-9452
- [ ] OFF-mode commands + raw output attached to CPESP-9452
- [ ] Pass/fail summary attached
- [ ] Any blocker logs attached with remediation notes

## Final Summary For Task Closure

- 6.1 ON validation: PASS / FAIL
- 6.2 OFF validation: PASS / FAIL
- 6.3 Evidence attached: YES / NO
- Follow-up actions (if any):
