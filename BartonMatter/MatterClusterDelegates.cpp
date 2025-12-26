/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#include "MatterClusterDelegates.h"
#include <lib/support/logging/CHIPLogging.h>
#include <linux/uinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/time.h>
#include <cstdlib>
#include <array>

// Ensure BUS_VIRTUAL is defined
#ifndef BUS_VIRTUAL
#define BUS_VIRTUAL 0x06
#endif

using namespace chip;
using namespace chip::app::Clusters;

namespace WPEFramework
{
    namespace Plugin
    {
        MatterKeypadInputDelegate::MatterKeypadInputDelegate()
        {
            ChipLogProgress(AppServer, "MatterKeypadInputDelegate created");
            if (!InitializeUinput())
            {
                ChipLogError(AppServer, "Failed to initialize uinput device");
            }
        }

        MatterKeypadInputDelegate::~MatterKeypadInputDelegate()
        {
            CleanupUinput();
        }

        bool MatterKeypadInputDelegate::InitializeUinput()
        {
            mUinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
            if (mUinputFd < 0)
            {
                ChipLogError(AppServer, "Failed to open /dev/uinput: %s", strerror(errno));
                return false;
            }

            // Enable key events (match VNC device capabilities)
            ioctl(mUinputFd, UI_SET_EVBIT, EV_KEY);
            ioctl(mUinputFd, UI_SET_EVBIT, EV_SYN);
            ioctl(mUinputFd, UI_SET_EVBIT, EV_MSC);  // Add MSC events like IR device has
            ioctl(mUinputFd, UI_SET_MSCBIT, MSC_SCAN);  // Scancode events

            // Enable all key codes we might use
            // Navigation keys
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_UP);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_DOWN);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_LEFT);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_RIGHT);
            // Enable multiple select key options
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_ENTER);   // Standard Enter/Return
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_KPENTER); // Keypad Enter
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_OK);      // Explicit OK button (352)
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_SELECT);  // Explicit Select button (353)
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_ESC);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_HOME);    // Menu/Guide
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_F2);      // Help
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_F9);      // Info
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_PAGEUP);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_PAGEDOWN);

            // Number keys
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_0);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_1);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_2);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_3);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_4);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_5);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_6);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_7);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_8);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_9);

            // Channel/Volume keys (need CTRL modifier for channel)
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_LEFTCTRL);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_KPPLUS);      // Volume up
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_KPMINUS);     // Volume down
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_KPASTERISK);  // Mute

            // Media control keys (function keys as per keySimulator)
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_F7);   // Record
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_F10);  // Rewind
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_F11);  // Play/Pause
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_F12);  // Fast Forward
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_S);    // Stop (with CTRL)
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_NEXT);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_PREVIOUS);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_EJECTCD);

            // Power
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_POWER);

            // Additional function keys
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_EPG);         // Electronic Program Guide
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_FAVORITES);

            // Setup device (match VNC daemon bus type and similar vendor ID)
            struct uinput_setup usetup;
            memset(&usetup, 0, sizeof(usetup));
            usetup.id.bustype = BUS_VIRTUAL;  // 0x0006 - same as VNC device
            usetup.id.vendor = 0x27d6;  // Similar to VNC (0x27d5) for Matter
            usetup.id.product = 0x6d74;  // 'mt' for Matter
            usetup.id.version = 1;
            strncpy(usetup.name, "matter-key-injector", UINPUT_MAX_NAME_SIZE);

            if (ioctl(mUinputFd, UI_DEV_SETUP, &usetup) < 0)
            {
                ChipLogError(AppServer, "Failed to setup uinput device: %s", strerror(errno));
                close(mUinputFd);
                mUinputFd = -1;
                return false;
            }

            if (ioctl(mUinputFd, UI_DEV_CREATE) < 0)
            {
                ChipLogError(AppServer, "Failed to create uinput device: %s", strerror(errno));
                close(mUinputFd);
                mUinputFd = -1;
                return false;
            }

            // will device be created immediately or this delay is necessary?
            usleep(50000);

            ChipLogProgress(AppServer, "Uinput device initialized successfully");
            return true;
        }

        void MatterKeypadInputDelegate::CleanupUinput()
        {
            if (mUinputFd >= 0)
            {
                ioctl(mUinputFd, UI_DEV_DESTROY);
                close(mUinputFd);
                mUinputFd = -1;
                ChipLogProgress(AppServer, "Uinput device cleaned up");
            }
        }

        void MatterKeypadInputDelegate::SendKeyEvent(int linuxKeyCode)
        {
            if (mUinputFd < 0)
            {
                ChipLogError(AppServer, "Uinput not initialized");
                return;
            }
	    /*
	     * the internal kernel input_struct will look something likee this
	     * struct input_event {
	     * 	struct timeval time;
	     *	short type;
	     *	short code;
	     *	int value;
	     * }
	     *
	     * */

            struct input_event ev;
            memset(&ev, 0, sizeof(ev));
            gettimeofday(&ev.time, NULL);

            // Key press
            ev.type = EV_KEY;
            ev.code = linuxKeyCode;
            ev.value = 1;
            ssize_t ret = write(mUinputFd, &ev, sizeof(ev));
            ChipLogProgress(AppServer, "Key press: code=%d, type=%d, value=%d, write_ret=%zd",
                          linuxKeyCode, ev.type, ev.value, ret);

            // Sync is necessary, else kernel keeps on waiting
            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            // 0.1ms delay to mimic the key press and release behaviour
            usleep(100);

            // Key release
            ev.type = EV_KEY;
            ev.code = linuxKeyCode;
            ev.value = 0;
            gettimeofday(&ev.time, NULL);
            ret = write(mUinputFd, &ev, sizeof(ev));
            ChipLogProgress(AppServer, "Key release: code=%d, type=%d, value=%d, write_ret=%zd",
                          linuxKeyCode, ev.type, ev.value, ret);

            // Sync
            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            ChipLogProgress(AppServer, "Sent key event: code=%d", linuxKeyCode);
        }

        void MatterKeypadInputDelegate::SendKeyWithModifier(int modifierKeyCode, int mainKeyCode)
        {
            if (mUinputFd < 0)
            {
                ChipLogError(AppServer, "Uinput not initialized");
                return;
            }

            struct input_event ev;
            memset(&ev, 0, sizeof(ev));

            // Press modifier (e.g., CTRL)
            gettimeofday(&ev.time, NULL);
            ev.type = EV_KEY;
            ev.code = modifierKeyCode;
            ev.value = 1;
            write(mUinputFd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            usleep(100);

            // Press main key
            gettimeofday(&ev.time, NULL);
            ev.type = EV_KEY;
            ev.code = mainKeyCode;
            ev.value = 1;
            write(mUinputFd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            usleep(100);

            // Release main key
            gettimeofday(&ev.time, NULL);
            ev.type = EV_KEY;
            ev.code = mainKeyCode;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            usleep(100);

            // Release modifier
            gettimeofday(&ev.time, NULL);
            ev.type = EV_KEY;
            ev.code = modifierKeyCode;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            ChipLogProgress(AppServer, "Sent key with modifier: modifier=%d, key=%d", modifierKeyCode, mainKeyCode);
        }

        void MatterKeypadInputDelegate::SendKeyHold(int linuxKeyCode, int holdDurationMs)
        {
            if (mUinputFd < 0)
            {
                ChipLogError(AppServer, "Uinput not initialized");
                return;
            }

            struct input_event ev;
            memset(&ev, 0, sizeof(ev));

            // Press key
            gettimeofday(&ev.time, NULL);
            ev.type = EV_KEY;
            ev.code = linuxKeyCode;
            ev.value = 1;
            write(mUinputFd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            // Hold the key for specified duration
            usleep(holdDurationMs * 1000);

            // Release key
            gettimeofday(&ev.time, NULL);
            ev.type = EV_KEY;
            ev.code = linuxKeyCode;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            ChipLogProgress(AppServer, "Sent key hold: code=%d, duration=%dms", linuxKeyCode, holdDurationMs);
        }

        void MatterKeypadInputDelegate::PressKey(int linuxKeyCode)
        {
            if (mUinputFd < 0)
            {
                ChipLogError(AppServer, "Uinput not initialized");
                return;
            }

            // Release any currently held key first
            ReleaseCurrentHeldKey();

            struct input_event ev;
            memset(&ev, 0, sizeof(ev));

            // Press key
            gettimeofday(&ev.time, NULL);
            ev.type = EV_KEY;
            ev.code = linuxKeyCode;
            ev.value = 1;
            write(mUinputFd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            mCurrentHeldKey = linuxKeyCode;
            ChipLogProgress(AppServer, "Key pressed and held: code=%d", linuxKeyCode);
        }

        void MatterKeypadInputDelegate::ReleaseKey(int linuxKeyCode)
        {
            if (mUinputFd < 0)
            {
                ChipLogError(AppServer, "Uinput not initialized");
                return;
            }

            struct input_event ev;
            memset(&ev, 0, sizeof(ev));

            // Release key
            gettimeofday(&ev.time, NULL);
            ev.type = EV_KEY;
            ev.code = linuxKeyCode;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            if (mCurrentHeldKey == linuxKeyCode)
            {
                mCurrentHeldKey = -1;
            }

            ChipLogProgress(AppServer, "Key released: code=%d", linuxKeyCode);
        }

        void MatterKeypadInputDelegate::ReleaseCurrentHeldKey()
        {
            if (mCurrentHeldKey >= 0)
            {
                ReleaseKey(mCurrentHeldKey);
            }
        }

        int MatterKeypadInputDelegate::GetLinuxKeyCode(const char* keyName)
        {
            // Map device key names to Linux input key codes
            // inspired from keySimulator's mapping table
            // Navigation
            if (strcmp(keyName, "up") == 0) return KEY_UP;
            if (strcmp(keyName, "down") == 0) return KEY_DOWN;
            if (strcmp(keyName, "left") == 0) return KEY_LEFT;
            if (strcmp(keyName, "right") == 0) return KEY_RIGHT;
            if (strcmp(keyName, "select") == 0) return KEY_ENTER;  // Maps to Qt Key_Return / Key_Select
            if (strcmp(keyName, "back") == 0) return KEY_ESC;  // KED_BACK -> KEY_ESC
            if (strcmp(keyName, "exit") == 0) return KEY_ESC;
            if (strcmp(keyName, "home") == 0) return KEY_HOME;  // KED_MENU/GUIDE -> KEY_HOME
            if (strcmp(keyName, "menu") == 0) return KEY_HOME;  // KED_MENU -> KEY_HOME
            if (strcmp(keyName, "info") == 0) return KEY_F9;    // KED_INFO -> KEY_F9
            if (strcmp(keyName, "help") == 0) return KEY_F2;    // KED_HELP -> KEY_F2
            if (strcmp(keyName, "pageup") == 0) return KEY_PAGEUP;
            if (strcmp(keyName, "pagedown") == 0) return KEY_PAGEDOWN;

            // Numbers
            if (strcmp(keyName, "0") == 0) return KEY_0;
            if (strcmp(keyName, "1") == 0) return KEY_1;
            if (strcmp(keyName, "2") == 0) return KEY_2;
            if (strcmp(keyName, "3") == 0) return KEY_3;
            if (strcmp(keyName, "4") == 0) return KEY_4;
            if (strcmp(keyName, "5") == 0) return KEY_5;
            if (strcmp(keyName, "6") == 0) return KEY_6;
            if (strcmp(keyName, "7") == 0) return KEY_7;
            if (strcmp(keyName, "8") == 0) return KEY_8;
            if (strcmp(keyName, "9") == 0) return KEY_9;

            // Volume (keypad keys)
            if (strcmp(keyName, "volup") == 0) return KEY_KPPLUS;
            if (strcmp(keyName, "voldown") == 0) return KEY_KPMINUS;
            if (strcmp(keyName, "mute") == 0) return KEY_KPASTERISK;

            // Media controls (using function keys as per keySimulator mapping)
            if (strcmp(keyName, "playpause") == 0) return KEY_F11;
            if (strcmp(keyName, "play") == 0) return KEY_F11;
            if (strcmp(keyName, "pause") == 0) return KEY_F11;
            if (strcmp(keyName, "stop") == 0) return KEY_S;  // Will use CTRL+S
            if (strcmp(keyName, "record") == 0) return KEY_F7;
            if (strcmp(keyName, "rewind") == 0) return KEY_F10;
            if (strcmp(keyName, "fastforward") == 0) return KEY_F12;
            if (strcmp(keyName, "forward") == 0) return KEY_NEXT;
            if (strcmp(keyName, "backward") == 0) return KEY_PREVIOUS;
            if (strcmp(keyName, "eject") == 0) return KEY_EJECTCD;

            // Power
            if (strcmp(keyName, "power") == 0) return KEY_POWER;

            // Special functions
            if (strcmp(keyName, "epg") == 0) return KEY_EPG;
            if (strcmp(keyName, "favorites") == 0) return KEY_FAVORITES;

            return -1;
        }

        void MatterKeypadInputDelegate::HandleSendKey(
            chip::app::CommandResponseHelper<KeypadInput::Commands::SendKeyResponse::Type> & helper,
            const KeypadInput::CECKeyCodeEnum & keyCode)
        {
            const char* keySimCmd = nullptr;
            bool useModifier = false;
            int modifierCode = -1;
            int mainKeyCode = -1;

            switch (keyCode)
            {
                // Basic navigation
                case KeypadInput::CECKeyCodeEnum::kUp:
                    keySimCmd = "up";
                    break;
                case KeypadInput::CECKeyCodeEnum::kDown:
                    keySimCmd = "down";
                    break;
                case KeypadInput::CECKeyCodeEnum::kLeft:
                    keySimCmd = "left";
                    break;
                case KeypadInput::CECKeyCodeEnum::kRight:
                    keySimCmd = "right";
                    break;
                case KeypadInput::CECKeyCodeEnum::kSelect:
                    keySimCmd = "select";
                    break;

                // Exit/Back
                case KeypadInput::CECKeyCodeEnum::kBackward:
                    keySimCmd = "back";
                    break;
                case KeypadInput::CECKeyCodeEnum::kExit:
                    keySimCmd = "exit";
                    break;

                // Menu navigation
                case KeypadInput::CECKeyCodeEnum::kRootMenu:
                    keySimCmd = "home";
                    break;
                case KeypadInput::CECKeyCodeEnum::kSetupMenu:
                    keySimCmd = "menu";
                    break;
                case KeypadInput::CECKeyCodeEnum::kContentsMenu:
                case KeypadInput::CECKeyCodeEnum::kFavoriteMenu:
                    keySimCmd = "menu";
                    break;
                case KeypadInput::CECKeyCodeEnum::kMediaTopMenu:
                case KeypadInput::CECKeyCodeEnum::kMediaContextSensitiveMenu:
                    keySimCmd = "menu";
                    break;

                // Display and help
                case KeypadInput::CECKeyCodeEnum::kDisplayInformation:
                    keySimCmd = "info";
                    break;
                case KeypadInput::CECKeyCodeEnum::kHelp:
                    keySimCmd = "help";
                    break;

                // Page navigation
                case KeypadInput::CECKeyCodeEnum::kPageUp:
                    keySimCmd = "pageup";
                    break;
                case KeypadInput::CECKeyCodeEnum::kPageDown:
                    keySimCmd = "pagedown";
                    break;

                // Number keys
                case KeypadInput::CECKeyCodeEnum::kNumber0OrNumber10:
                    keySimCmd = "0";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers1:
                    keySimCmd = "1";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers2:
                    keySimCmd = "2";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers3:
                    keySimCmd = "3";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers4:
                    keySimCmd = "4";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers5:
                    keySimCmd = "5";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers6:
                    keySimCmd = "6";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers7:
                    keySimCmd = "7";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers8:
                    keySimCmd = "8";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers9:
                    keySimCmd = "9";
                    break;
                case KeypadInput::CECKeyCodeEnum::kEnter:
                    keySimCmd = "select";
                    break;

                // Channel control (CTRL + UP/DOWN)
                case KeypadInput::CECKeyCodeEnum::kChannelUp:
                    useModifier = true;
                    modifierCode = KEY_LEFTCTRL;
                    mainKeyCode = KEY_UP;
                    break;
                case KeypadInput::CECKeyCodeEnum::kChannelDown:
                    useModifier = true;
                    modifierCode = KEY_LEFTCTRL;
                    mainKeyCode = KEY_DOWN;
                    break;

                // Volume control
                case KeypadInput::CECKeyCodeEnum::kVolumeUp:
                    keySimCmd = "volup";
                    break;
                case KeypadInput::CECKeyCodeEnum::kVolumeDown:
                    keySimCmd = "voldown";
                    break;
                case KeypadInput::CECKeyCodeEnum::kMute:
                case KeypadInput::CECKeyCodeEnum::kMuteFunction:
                    keySimCmd = "mute";
                    break;

                // Media playback controls
                case KeypadInput::CECKeyCodeEnum::kPlay:
                case KeypadInput::CECKeyCodeEnum::kPlayFunction:
                    // Release any held key before play
                    useModifier = true;
                    modifierCode = -3;  // Special marker for release + play
                    mainKeyCode = -1;
                    keySimCmd = "play";
                    break;
                case KeypadInput::CECKeyCodeEnum::kPause:
                case KeypadInput::CECKeyCodeEnum::kPausePlayFunction:
                    // Release any held key before pause
                    useModifier = true;
                    modifierCode = -3;  // Special marker for release + pause
                    mainKeyCode = -1;
                    keySimCmd = "pause";
                    break;
                case KeypadInput::CECKeyCodeEnum::kStop:
                case KeypadInput::CECKeyCodeEnum::kStopFunction:
                    // Stop uses CTRL+S
                    useModifier = true;
                    modifierCode = KEY_LEFTCTRL;
                    mainKeyCode = KEY_S;
                    break;
                case KeypadInput::CECKeyCodeEnum::kRecord:
                case KeypadInput::CECKeyCodeEnum::kRecordFunction:
                    keySimCmd = "record";
                    break;
                case KeypadInput::CECKeyCodeEnum::kRewind:
                    // Press and hold LEFT key indefinitely
                    useModifier = true;
                    modifierCode = -2;  // Special marker for press and hold
                    mainKeyCode = KEY_LEFT;
                    break;
                case KeypadInput::CECKeyCodeEnum::kFastForward:
                    // Press and hold RIGHT key indefinitely
                    useModifier = true;
                    modifierCode = -2;  // Special marker for press and hold
                    mainKeyCode = KEY_RIGHT;
                    break;
                case KeypadInput::CECKeyCodeEnum::kForward:
                    keySimCmd = "forward";
                    break;
                case KeypadInput::CECKeyCodeEnum::kEject:
                    keySimCmd = "eject";
                    break;

                // Power
                case KeypadInput::CECKeyCodeEnum::kPower:
                case KeypadInput::CECKeyCodeEnum::kPowerToggleFunction:
                case KeypadInput::CECKeyCodeEnum::kPowerOnFunction:
                case KeypadInput::CECKeyCodeEnum::kPowerOffFunction:
                    keySimCmd = "power";
                    break;

                // EPG
                case KeypadInput::CECKeyCodeEnum::kElectronicProgramGuide:
                    keySimCmd = "epg";
                    break;

                default:
                    ChipLogProgress(AppServer, "Key code 0x%02x not mapped", static_cast<uint8_t>(keyCode));
                    break;
            }

            // its better to send the response as soon as we reeceive the key
            KeypadInput::Commands::SendKeyResponse::Type response;
            response.status = KeypadInput::StatusEnum::kSuccess;
            helper.Success(response);

            // Inject key via uinput
            if (useModifier && modifierCode == -2 && mainKeyCode >= 0)
            {
                // Fast forward/Rewind: Press and hold key indefinitely
                PressKey(mainKeyCode);
                ChipLogProgress(AppServer, "Key pressed and holding: key=%d", mainKeyCode);
            }
            else if (useModifier && modifierCode == -3)
            {
                // Play/Pause: Release any held key first
                ReleaseCurrentHeldKey();
                // Then send the play/pause command
                if (keySimCmd != nullptr)
                {
                    int linuxKeyCode = GetLinuxKeyCode(keySimCmd);
                    if (linuxKeyCode >= 0)
                    {
                        SendKeyEvent(linuxKeyCode);
                        ChipLogProgress(AppServer, "Released held key and injected: %s (code=%d)", keySimCmd, linuxKeyCode);
                    }
                }
            }
            else if (useModifier && modifierCode >= 0 && mainKeyCode >= 0)
            {
                // Channel keys use CTRL+UP/DOWN
                SendKeyWithModifier(modifierCode, mainKeyCode);
                ChipLogProgress(AppServer, "Injected key with modifier: mod=%d, key=%d", modifierCode, mainKeyCode);
            }
            else if (keySimCmd != nullptr)
            {
                int linuxKeyCode = GetLinuxKeyCode(keySimCmd);
                if (linuxKeyCode >= 0)
                {
                    SendKeyEvent(linuxKeyCode);
                    ChipLogProgress(AppServer, "Injected key: %s (code=%d)", keySimCmd, linuxKeyCode);
                }
                else
                {
                    ChipLogError(AppServer, "Failed to map key: %s", keySimCmd);
                }
            }
        }

        uint32_t MatterKeypadInputDelegate::GetFeatureMap(chip::EndpointId endpoint)
        {
            // Enable all key features: NavigationKeyCodes, LocationKeys, NumberKeys
            return 0x07; // Bits 0,1,2 set
        }

        // MatterApplicationLauncherDelegate Implementation

        MatterApplicationLauncherDelegate::MatterApplicationLauncherDelegate()
        {
            ChipLogProgress(AppServer, "MatterApplicationLauncherDelegate created");
        }

        void MatterApplicationLauncherDelegate::HandleLaunchApp(
            chip::app::CommandResponseHelper<ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
            const chip::ByteSpan & data,
            const ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application)
        {
            ChipLogProgress(AppServer, "HandleLaunchApp: catalogVendorId=%d, applicationId=%.*s",
                          application.catalogVendorID,
                          static_cast<int>(application.applicationID.size()),
                          application.applicationID.data());

            ApplicationLauncher::Commands::LauncherResponse::Type response;

            // Extract application ID as string
            std::string appId(reinterpret_cast<const char*>(application.applicationID.data()),
                            application.applicationID.size());

            // Build Thunder API command to launch app
            std::string command = "curl -X POST \"http://127.0.0.1:9005/as/apps/action/launch?appId=" + appId + "\" -d '' 2>&1";

            ChipLogProgress(AppServer, "Launching app with command: %s", command.c_str());

            // Execute the command
            std::array<char, 128> buffer;
            std::string result;
            FILE* pipe = popen(command.c_str(), "r");

            if (pipe)
            {
                while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                {
                    result += buffer.data();
                }
                int exitCode = pclose(pipe);

                if (exitCode == 0)
                {
                    response.status = ApplicationLauncher::StatusEnum::kSuccess;
                    ChipLogProgress(AppServer, "Application launched successfully: %s", appId.c_str());
                }
                else
                {
                    response.status = ApplicationLauncher::StatusEnum::kSystemBusy;
                    ChipLogError(AppServer, "Failed to launch app %s, exit code: %d, output: %s",
                               appId.c_str(), exitCode, result.c_str());
                }
            }
            else
            {
                response.status = ApplicationLauncher::StatusEnum::kSystemBusy;
                ChipLogError(AppServer, "Failed to execute launch command for %s", appId.c_str());
            }

            helper.Success(response);
        }

        void MatterApplicationLauncherDelegate::HandleStopApp(
            chip::app::CommandResponseHelper<ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
            const ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application)
        {
            ChipLogProgress(AppServer, "HandleStopApp: catalogVendorId=%d, applicationId=%.*s",
                          application.catalogVendorID,
                          static_cast<int>(application.applicationID.size()),
                          application.applicationID.data());

            ApplicationLauncher::Commands::LauncherResponse::Type response;

            // Extract application ID as string
            std::string appId(reinterpret_cast<const char*>(application.applicationID.data()),
                            application.applicationID.size());

            // Build Thunder API command to close app
            std::string command = "curl -X POST \"http://127.0.0.1:9005/as/apps/action/close?appId=" + appId + "\" -d '' 2>&1";

            ChipLogProgress(AppServer, "Closing app with command: %s", command.c_str());

            // Execute the command
            std::array<char, 128> buffer;
            std::string result;
            FILE* pipe = popen(command.c_str(), "r");

            if (pipe)
            {
                while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                {
                    result += buffer.data();
                }
                int exitCode = pclose(pipe);

                if (exitCode == 0)
                {
                    response.status = ApplicationLauncher::StatusEnum::kSuccess;
                    ChipLogProgress(AppServer, "Application closed successfully: %s", appId.c_str());
                }
                else
                {
                    response.status = ApplicationLauncher::StatusEnum::kSystemBusy;
                    ChipLogError(AppServer, "Failed to close app %s, exit code: %d, output: %s",
                               appId.c_str(), exitCode, result.c_str());
                }
            }
            else
            {
                response.status = ApplicationLauncher::StatusEnum::kSystemBusy;
                ChipLogError(AppServer, "Failed to execute close command for %s", appId.c_str());
            }

            helper.Success(response);
        }

        void MatterApplicationLauncherDelegate::HandleHideApp(
            chip::app::CommandResponseHelper<ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
            const ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application)
        {
            ChipLogProgress(AppServer, "HandleHideApp: catalogVendorId=%d, applicationId=%.*s",
                          application.catalogVendorID,
                          static_cast<int>(application.applicationID.size()),
                          application.applicationID.data());

            ApplicationLauncher::Commands::LauncherResponse::Type response;

            // TODO: Integrate with Thunder application management
            response.status = ApplicationLauncher::StatusEnum::kSuccess;

            helper.Success(response);

            // TODO: Hide/minimize the application via Thunder plugin
            ChipLogProgress(AppServer, "Application hide would be executed here");
        }        CHIP_ERROR MatterApplicationLauncherDelegate::HandleGetCatalogList(chip::app::AttributeValueEncoder & encoder)
        {
            // Return list of supported catalog vendor IDs
            // 0 = Content platform (CSA specification)
            return encoder.EncodeList([](const auto & listEncoder) -> CHIP_ERROR {
                // Add catalog vendor ID 0 (CSA specification)
                ReturnErrorOnFailure(listEncoder.Encode(static_cast<uint16_t>(0)));
                return CHIP_NO_ERROR;
            });
        }

        // MatterClusterDelegateManager Implementation

        MatterClusterDelegateManager& MatterClusterDelegateManager::GetInstance()
        {
            static MatterClusterDelegateManager instance;
            return instance;
        }

        void MatterClusterDelegateManager::Initialize()
        {
            if (mInitialized)
            {
                return;
            }

            // Create KeypadInput delegate
            mKeypadInputDelegate = std::make_unique<MatterKeypadInputDelegate>();

            // Register delegate for KeypadInput cluster on endpoint 3 (static configuration from ZAP)
            KeypadInput::SetDefaultDelegate(3, mKeypadInputDelegate.get());
            mRegisteredEndpoints.push_back(3);

            // Create ApplicationLauncher delegate
            mApplicationLauncherDelegate = std::make_unique<MatterApplicationLauncherDelegate>();

            // Register delegate for ApplicationLauncher cluster on endpoint 3
            ApplicationLauncher::SetDefaultDelegate(3, mApplicationLauncherDelegate.get());

            mInitialized = true;
            ChipLogProgress(AppServer, "KeypadInput and ApplicationLauncher delegates registered for endpoint 3");
        }



        void MatterClusterDelegateManager::Shutdown()
        {
            if (!mInitialized)
            {
                return;
            }

            // Unregister delegates
            for (chip::EndpointId ep : mRegisteredEndpoints)
            {
                KeypadInput::SetDefaultDelegate(ep, nullptr);
                ApplicationLauncher::SetDefaultDelegate(ep, nullptr);
            }
            mRegisteredEndpoints.clear();

            // Cleanup NetworkCommissioning
            mNetworkCommissioningInstance.reset();
            mWiFiDriver.reset();

            // Cleanup cluster delegates
            mKeypadInputDelegate.reset();
            mApplicationLauncherDelegate.reset();
            mInitialized = false;
        }

        void MatterClusterDelegateManager::InitializeNetworkCommissioning()
        {
            ChipLogProgress(AppServer, "Initializing NetworkCommissioning cluster on endpoint 0");

            // Create WiFi driver
            mWiFiDriver = std::make_unique<WiFiDriver>();

            // Create NetworkCommissioning instance on endpoint 0
            mNetworkCommissioningInstance = std::make_unique<NetworkCommissioning::Instance>(0, mWiFiDriver.get());

            CHIP_ERROR err = mNetworkCommissioningInstance->Init();
            if (err != CHIP_NO_ERROR)
            {
                ChipLogError(AppServer, "Failed to initialize NetworkCommissioning instance: %s", chip::ErrorStr(err));
                return;
            }

            ChipLogProgress(AppServer, "NetworkCommissioning cluster initialized successfully with WiFi driver");
        }

        // WiFiDriver Implementation

        CHIP_ERROR WiFiDriver::Init(chip::DeviceLayer::NetworkCommissioning::Internal::BaseDriver::NetworkStatusChangeCallback * callback)
        {
            mStatusChangeCallback = callback;
            ChipLogProgress(AppServer, "WiFiDriver initialized");
            return CHIP_NO_ERROR;
        }

        void WiFiDriver::Shutdown()
        {
            mStatusChangeCallback = nullptr;
        }

        CHIP_ERROR WiFiDriver::CommitConfiguration()
        {
            ChipLogProgress(AppServer, "WiFiDriver: CommitConfiguration called");
            return CHIP_NO_ERROR;
        }

        CHIP_ERROR WiFiDriver::RevertConfiguration()
        {
            ChipLogProgress(AppServer, "WiFiDriver: RevertConfiguration called");
            return CHIP_NO_ERROR;
        }

        WiFiDriver::Status WiFiDriver::RemoveNetwork(chip::ByteSpan networkId, chip::MutableCharSpan & outDebugText, uint8_t & outNetworkIndex)
        {
            ChipLogProgress(AppServer, "WiFiDriver: RemoveNetwork called");
            return Status::kSuccess;
        }

        WiFiDriver::Status WiFiDriver::ReorderNetwork(chip::ByteSpan networkId, uint8_t index, chip::MutableCharSpan & outDebugText)
        {
            ChipLogProgress(AppServer, "WiFiDriver: ReorderNetwork called");
            return Status::kSuccess;
        }

        void WiFiDriver::ConnectNetwork(chip::ByteSpan networkId, ConnectCallback * callback)
        {
            ChipLogProgress(AppServer, "WiFiDriver: ConnectNetwork called");
            // Device is already connected to WiFi via system configuration
            if (callback)
            {
                callback->OnResult(Status::kSuccess, chip::CharSpan(), 0);
            }
        }

        void WiFiDriver::ScanNetworks(chip::ByteSpan ssid, ScanCallback * callback)
        {
            ChipLogProgress(AppServer, "WiFiDriver: ScanNetworks called");
            // Return empty scan results - device manages WiFi at OS level
            if (callback)
            {
                callback->OnFinished(Status::kSuccess, chip::CharSpan(), nullptr);
            }
        }

        CHIP_ERROR WiFiDriver::AddOrUpdateNetwork(chip::ByteSpan ssid, chip::ByteSpan credentials, chip::MutableCharSpan & outDebugText, uint8_t & outNetworkIndex)
        {
            ChipLogProgress(AppServer, "WiFiDriver: AddOrUpdateNetwork called (SSID len=%u)", static_cast<unsigned>(ssid.size()));
            // Device WiFi is managed at OS level, but accept the configuration for Matter commissioning
            outNetworkIndex = 0;
            return CHIP_NO_ERROR;
        }

        void WiFiDriver::OnNetworkStatusChange()
        {
            ChipLogProgress(AppServer, "WiFiDriver: OnNetworkStatusChange called");
        }

        chip::BitFlags<chip::app::Clusters::NetworkCommissioning::WiFiSecurityBitmap> WiFiDriver::GetSecurityTypes()
        {
            return chip::BitFlags<chip::app::Clusters::NetworkCommissioning::WiFiSecurityBitmap>(
                chip::app::Clusters::NetworkCommissioning::WiFiSecurityBitmap::kWpa2Personal |
                chip::app::Clusters::NetworkCommissioning::WiFiSecurityBitmap::kWpa3Personal);
        }

        chip::BitFlags<chip::app::Clusters::NetworkCommissioning::WiFiBandBitmap> WiFiDriver::GetWiFiBands()
        {
            return chip::BitFlags<chip::app::Clusters::NetworkCommissioning::WiFiBandBitmap>(
                chip::app::Clusters::NetworkCommissioning::WiFiBandBitmap::k2g4 |
                chip::app::Clusters::NetworkCommissioning::WiFiBandBitmap::k5g);
        }

    } // namespace Plugin
} // namespace WPEFramework


