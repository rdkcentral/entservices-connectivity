# GStreamer Plugin Testing Commands

## Quick Reference

**Service Name:** `org.rdk.GStreamer`  
**API Version:** `1`  
**Thunder Port:** `9998`

---

## 1. Activate Plugin

```bash
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.activate", "params":{"callsign":"org.rdk.GStreamer"}}' http://127.0.0.1:9998/jsonrpc
```

**Expected Response:**
```json
{"jsonrpc":"2.0","id":"3","result":null}
```

---

## 2. Check Plugin Status

```bash
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.status@org.rdk.GStreamer"}' http://127.0.0.1:9998/jsonrpc
```

**Expected Response:**
```json
{"jsonrpc":"2.0","id":"3","result":[{"callsign":"org.rdk.GStreamer","state":"activated",...}]}
```

---

## 3. Play Video

```bash
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.play", "params":{}}' http://127.0.0.1:9998/jsonrpc
```

**Expected Response:**
```json
{"jsonrpc":"2.0","id":"3","result":{"success":true,"message":"Pipeline started playing"}}
```

**What Happens:** Video starts playing (Sintel trailer from GStreamer website)

---

## 4. Pause Video

```bash
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.pause", "params":{}}' http://127.0.0.1:9998/jsonrpc
```

**Expected Response:**
```json
{"jsonrpc":"2.0","id":"3","result":{"success":true,"message":"Pipeline paused"}}
```

**What Happens:** Video pauses at current position

---

## 5. Resume Playing

```bash
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.play", "params":{}}' http://127.0.0.1:9998/jsonrpc
```

**What Happens:** Video resumes from paused position

---

## 6. Stop and Cleanup (Quit)

```bash
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.quit", "params":{}}' http://127.0.0.1:9998/jsonrpc
```

**Expected Response:**
```json
{"jsonrpc":"2.0","id":"3","result":{"success":true,"message":"Pipeline stopped and cleaned up"}}
```

**What Happens:** Pipeline destroyed, all resources freed

---

## 7. Deactivate Plugin

```bash
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.deactivate", "params":{"callsign":"org.rdk.GStreamer"}}' http://127.0.0.1:9998/jsonrpc
```

**Expected Response:**
```json
{"jsonrpc":"2.0","id":"3","result":null}
```

---

## Complete Test Script (Bash)

```bash
#!/bin/bash

echo "=== GStreamer Plugin Test Sequence ==="
echo ""

echo "1. Activating plugin..."
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.activate", "params":{"callsign":"org.rdk.GStreamer"}}' http://127.0.0.1:9998/jsonrpc
echo ""
sleep 2

echo "2. Checking status..."
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.status@org.rdk.GStreamer"}' http://127.0.0.1:9998/jsonrpc
echo ""
sleep 1

echo "3. Starting playback..."
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.play", "params":{}}' http://127.0.0.1:9998/jsonrpc
echo ""
echo "   (Video should be playing now)"
sleep 10

echo "4. Pausing video..."
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.pause", "params":{}}' http://127.0.0.1:9998/jsonrpc
echo ""
echo "   (Video should be paused)"
sleep 3

echo "5. Resuming playback..."
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.play", "params":{}}' http://127.0.0.1:9998/jsonrpc
echo ""
sleep 10

echo "6. Stopping and cleaning up..."
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.quit", "params":{}}' http://127.0.0.1:9998/jsonrpc
echo ""
sleep 1

echo "7. Deactivating plugin..."
curl -d '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.deactivate", "params":{"callsign":"org.rdk.GStreamer"}}' http://127.0.0.1:9998/jsonrpc
echo ""

echo "=== Test Complete ==="
```

---

## PowerShell Version (Windows)

```powershell
Write-Host "=== GStreamer Plugin Test Sequence ===" -ForegroundColor Cyan
Write-Host ""

Write-Host "1. Activating plugin..." -ForegroundColor Yellow
Invoke-RestMethod -Uri "http://127.0.0.1:9998/jsonrpc" -Method Post -Body '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.activate", "params":{"callsign":"org.rdk.GStreamer"}}' -ContentType "application/json"
Start-Sleep -Seconds 2

Write-Host "2. Checking status..." -ForegroundColor Yellow
Invoke-RestMethod -Uri "http://127.0.0.1:9998/jsonrpc" -Method Post -Body '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.status@org.rdk.GStreamer"}' -ContentType "application/json"
Start-Sleep -Seconds 1

Write-Host "3. Starting playback..." -ForegroundColor Yellow
Invoke-RestMethod -Uri "http://127.0.0.1:9998/jsonrpc" -Method Post -Body '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.play", "params":{}}' -ContentType "application/json"
Write-Host "   (Video should be playing now)" -ForegroundColor Green
Start-Sleep -Seconds 10

Write-Host "4. Pausing video..." -ForegroundColor Yellow
Invoke-RestMethod -Uri "http://127.0.0.1:9998/jsonrpc" -Method Post -Body '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.pause", "params":{}}' -ContentType "application/json"
Write-Host "   (Video should be paused)" -ForegroundColor Green
Start-Sleep -Seconds 3

Write-Host "5. Resuming playback..." -ForegroundColor Yellow
Invoke-RestMethod -Uri "http://127.0.0.1:9998/jsonrpc" -Method Post -Body '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.play", "params":{}}' -ContentType "application/json"
Start-Sleep -Seconds 10

Write-Host "6. Stopping and cleaning up..." -ForegroundColor Yellow
Invoke-RestMethod -Uri "http://127.0.0.1:9998/jsonrpc" -Method Post -Body '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.GStreamer.1.quit", "params":{}}' -ContentType "application/json"
Start-Sleep -Seconds 1

Write-Host "7. Deactivating plugin..." -ForegroundColor Yellow
Invoke-RestMethod -Uri "http://127.0.0.1:9998/jsonrpc" -Method Post -Body '{"jsonrpc":"2.0", "id":"3", "method":"Controller.1.deactivate", "params":{"callsign":"org.rdk.GStreamer"}}' -ContentType "application/json"

Write-Host "=== Test Complete ===" -ForegroundColor Cyan
```

---

## On-Device Verification Commands

### Check if GStreamer libraries are installed:
```bash
ls -la /usr/lib/libgstreamer-1.0.so*
ls /usr/lib/gstreamer-1.0/
```

### Check if plugin is installed:
```bash
ls -la /usr/lib/wpeframework/plugins/libWPEFrameworkGStreamer.so
```

### Check library dependencies:
```bash
ldd /usr/lib/wpeframework/plugins/libWPEFrameworkGStreamer.so
```

### Check Thunder configuration:
```bash
ls /etc/WPEFramework/plugins/GStreamer.json
cat /etc/WPEFramework/plugins/GStreamer.json
```

### Check Thunder logs:
```bash
journalctl -u wpeframework -f
# or
tail -f /var/log/wpeframework.log

# When you call 'play', look for:
# - "westerossink not available..." messages
# - Any GStreamer errors about video output
# - "Pipeline state changed" messages
```

### Check which video sink was actually used:
```bash
# After calling play, check running GStreamer pipelines
ps aux | grep gst
pgrep -a gst

# Or query the pipeline state via GStreamer
gst-launch-1.0 playbin uri=file:///dev/null ! fakesink
```

### Test GStreamer directly:
```bash
gst-inspect-1.0 --version
gst-inspect-1.0 uridecodebin
gst-launch-1.0 playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm
```

---

## Troubleshooting

### ⚠️ AUDIO WORKS BUT NO VIDEO ON TV (Most Common Issue!)

**Problem:** You can hear audio but see no video on the TV screen.

**Cause:** Wrong video sink. `autovideosink` doesn't work on most STBs/RDK devices.

**Solution - Check which video sink your device uses:**
```bash
# On the device, check which video sinks are available
gst-inspect-1.0 | grep -i "sink" | grep -i "video"

# Expected output (one of these):
# westerossink      ← Most RDK devices
# waylandsink       ← Wayland-based devices
# brcmvideosink     ← Broadcom STBs
# amlvideosink      ← Amlogic STBs
```

**Verify the plugin is trying correct sinks:**
```bash
# Check Thunder logs when you call play
journalctl -u wpeframework -f | grep -i "sink"

# You should see lines like:
# "westerossink not available, trying waylandsink..."
# Or: "Using westerossink for video output"
```

**Manual GStreamer test with correct sink:**
```bash
# Test with westerossink (most common for RDK)
gst-launch-1.0 playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm video-sink=westerossink

# Test with waylandsink
gst-launch-1.0 playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm video-sink=waylandsink

# If video shows with one of these, that's the sink your device needs
```

**Check display environment:**
```bash
echo $WAYLAND_DISPLAY
echo $DISPLAY
env | grep -i display
```

---

### If plugin fails to activate:
```bash
# Check Thunder is running
systemctl status wpeframework

# Check logs for errors
journalctl -u wpeframework -n 100

# Verify plugin file exists
ls -la /usr/lib/wpeframework/plugins/libWPEFrameworkGStreamer.so
```

### If video doesn't play:
```bash
# Check GStreamer plugins
gst-inspect-1.0 | grep -E 'audio|video'

# Test pipeline manually
gst-launch-1.0 uridecodebin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm ! autovideosink
```

### If "not found" errors:
```bash
# Check library paths
echo $LD_LIBRARY_PATH
ldconfig -p | grep gstreamer
```

---

## API Methods Summary

| Method | Parameters | Description |
|--------|-----------|-------------|
| `org.rdk.GStreamer.1.play` | `{}` | Start/resume video playback |
| `org.rdk.GStreamer.1.pause` | `{}` | Pause video playback |
| `org.rdk.GStreamer.1.quit` | `{}` | Stop and cleanup pipeline |

---

## Video Information

- **URL:** https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm
- **Format:** WebM (VP8 video, Vorbis audio)
- **Resolution:** 480p
- **Duration:** ~53 seconds
- **Description:** Sintel animated short film trailer

---

**Created:** February 24, 2026  
**Plugin Version:** 1.0.0  
**Thunder API:** JSON-RPC 2.0
