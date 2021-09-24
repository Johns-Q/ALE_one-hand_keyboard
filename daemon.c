///
///	@file daemon.c	@brief	ALE one-hand keyboard handler daemon.
///
///	Copyright (c) 2007,2009 by Lutz Sammer.	 All Rights Reserved.
///
///	Contributor(s):
///
///	This file is part of ALE one-hand keyboard
///
///	This program is free software; you can redistribute it and/or modify
///	it under the terms of the GNU General Public License as published by
///	the Free Software Foundation; only version 2 of the License.
///
///	This program is distributed in the hope that it will be useful,
///	but WITHOUT ANY WARRANTY; without even the implied warranty of
///	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///	GNU General Public License for more details.
///
///	$Id$
////////////////////////////////////////////////////////////////////////////

///
///	@defgroup daemon The aohk daemon module.
///
///	Uses aohk module to filter input devices.
///
///	@see aohkd.1 howto use the keyboard daemon.
///
///	@todo
///		- touchpad driver
///		- passthrough of touchpad driver
///		- usb devices which only support report(s) drivers
///		- builtin bluetooth virtual keyboard emulation
/// @{

#include <linux/input.h>
#include <linux/uinput.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <poll.h>

#include "aohk.h"
#include "uinput.h"

////////////////////////////////////////////////////////////////////////////

#define SYSTEM_VENDOR_ID	0x0001	///< system vendor
#define PS2_KEYBOARD_ID		0x0001	///< system keyboard

#define BELKIN_VENDOR_ID	0x050D	///< belkin's vendor ID
#define NOSTROMO_N52_ID		0x0815	///< nostromo n52 USB ID

#define CHERRY_VENDOR_ID	0x046A	///< cherry's vendor ID
#define KEYPAD_ID		0x0014	///< keypad G84-4700 USB ID

#define FUJSI_VENDOR_ID		0x0BF8	///< fujitsu siemens vendor ID
#define KEYPAD2_ID		0x1004	///< keypad USB ID

#define SAITEK_VENDOR_ID	0x06A3	///< saitek's vendor ID
#define PGCU_ID			0x80C0	///< pro gamer command unit USB ID

#define APPLE_VENDOR_ID		0x05AC	///< apple vendor ID
#define AWKB_ID			0x0209	///< wireless keyboard BT ID
#define AAKB_ID			0x021E	///< alu keyboard USB ID
#define AAWKB_ID		0x022D	///< alu Wireless keyboard BT ID

#define	LOGITECH_VENDOR_ID	0x046D	///< logitech vendoor ID
#define G13_ID			0xC21C	///< G13 USB keyboard ID

///
///	Mapping from keys to internal key for nostromo N52.
///
///	End marked with KEY_RESERVED
///
static const int N52ConvertTable[] = {
    KEY_SPACE, AOHK_KEY_0,
    KEY_Z, AOHK_KEY_1,
    KEY_X, AOHK_KEY_2,
    KEY_C, AOHK_KEY_3,
    KEY_A, AOHK_KEY_4,
    KEY_S, AOHK_KEY_5,
    KEY_D, AOHK_KEY_6,
    KEY_Q, AOHK_KEY_7,
    KEY_W, AOHK_KEY_8,
    KEY_E, AOHK_KEY_9,
    KEY_LEFTSHIFT, AOHK_KEY_HASH,

    KEY_LEFTALT, AOHK_KEY_STAR,

    KEY_TAB, AOHK_KEY_USR_1,
    KEY_CAPSLOCK, AOHK_KEY_USR_2,
    KEY_R, AOHK_KEY_USR_3,
    //KEY_F, AOHK_KEY_USR_4,
    //KEY_LEFT, AOHK_KEY_USR_5,
    //KEY_UP, AOHK_KEY_USR_6,
    //KEY_RIGHT, AOHK_KEY_USR_7,
    //KEY_DOWN, AOHK_KEY_USR_8,
    KEY_F, AOHK_KEY_SPECIAL,
    KEY_RESERVED, KEY_RESERVED
};

///
///	Mapping from keys to internal key for normal keyboard.
///	Typing on the left side of keyboard with left hand.
///
///	End marked with KEY_RESERVED
///
static const int PC102ConvertTable[] = {
    KEY_SPACE, AOHK_KEY_0,
    KEY_Z, AOHK_KEY_1,
    KEY_X, AOHK_KEY_2,
    KEY_C, AOHK_KEY_3,
    KEY_A, AOHK_KEY_4,
    KEY_S, AOHK_KEY_5,
    KEY_D, AOHK_KEY_6,
    KEY_Q, AOHK_KEY_7,
    KEY_W, AOHK_KEY_8,
    KEY_E, AOHK_KEY_9,
    KEY_LEFTSHIFT, AOHK_KEY_HASH,
    KEY_CAPSLOCK, AOHK_KEY_HASH,
    KEY_LEFTCTRL, AOHK_KEY_HASH,
    KEY_102ND, AOHK_KEY_HASH,

    KEY_LEFTALT, AOHK_KEY_STAR,
    KEY_LEFTMETA, AOHK_KEY_STAR,

    KEY_TAB, AOHK_KEY_USR_1,
    KEY_1, AOHK_KEY_USR_2,
    KEY_2, AOHK_KEY_USR_3,
    KEY_3, AOHK_KEY_USR_4,
    KEY_4, AOHK_KEY_USR_5,
    KEY_R, AOHK_KEY_USR_6,
    KEY_F, AOHK_KEY_USR_7,
    KEY_V, AOHK_KEY_USR_8,
    KEY_GRAVE, AOHK_KEY_SPECIAL,
    KEY_RESERVED, KEY_RESERVED
};

///
///	Mapping from keys to internal key for normal keyboard.
///	Typing with a number/key pad.
///
///	End marked with KEY_RESERVED
///
static const int NumpadConvertTable[] = {
    KEY_KPENTER, AOHK_KEY_0,
    KEY_KP1, AOHK_KEY_1,
    KEY_KP2, AOHK_KEY_2,
    KEY_KP3, AOHK_KEY_3,
    KEY_KP4, AOHK_KEY_4,
    KEY_KP5, AOHK_KEY_5,
    KEY_KP6, AOHK_KEY_6,
    KEY_KP7, AOHK_KEY_7,
    KEY_KP8, AOHK_KEY_8,
    KEY_KP9, AOHK_KEY_9,
    KEY_KP0, AOHK_KEY_HASH,

    KEY_KPDOT, AOHK_KEY_STAR,
    KEY_KPCOMMA, AOHK_KEY_STAR,

    KEY_KPSLASH, AOHK_KEY_USR_1,
    KEY_KPASTERISK, AOHK_KEY_USR_2,
    KEY_KPMINUS, AOHK_KEY_USR_3,
    KEY_KPPLUS, AOHK_KEY_USR_4,

    KEY_ESC, AOHK_KEY_USR_5,		// extra keys on cherry keypad
    KEY_LEFTCTRL, AOHK_KEY_USR_6,
    KEY_LEFTALT, AOHK_KEY_USR_7,
    KEY_BACKSPACE, AOHK_KEY_USR_8,

    KEY_NUMLOCK, AOHK_KEY_SPECIAL,

    KEY_RESERVED, KEY_RESERVED
};

///
///	Mapping from keys to internal key for normal keyboard.
///	Typing with a number/key pad.
///
///	End marked with KEY_RESERVED
///
static const int Numpad2ConvertTable[] = {
    KEY_KPENTER, AOHK_KEY_0,
    KEY_KP1, AOHK_KEY_1,
    KEY_KP2, AOHK_KEY_2,
    KEY_KP3, AOHK_KEY_3,
    KEY_KP4, AOHK_KEY_4,
    KEY_KP5, AOHK_KEY_5,
    KEY_KP6, AOHK_KEY_6,
    KEY_KP7, AOHK_KEY_7,
    KEY_KP8, AOHK_KEY_8,
    KEY_KP9, AOHK_KEY_9,
    KEY_KP0, AOHK_KEY_HASH,

    KEY_KPDOT, AOHK_KEY_STAR,
    KEY_KPCOMMA, AOHK_KEY_STAR,

    KEY_KPSLASH, AOHK_KEY_USR_1,
    KEY_KPASTERISK, AOHK_KEY_USR_2,
    KEY_KPMINUS, AOHK_KEY_USR_3,
    KEY_KPPLUS, AOHK_KEY_USR_4,

    KEY_NUMLOCK, AOHK_KEY_SPECIAL,

    KEY_RESERVED, KEY_RESERVED
};

///
///	Mapping from keys to internal key for Pro Gamer Command Unit.
///
///	End marked with KEY_RESERVED
///
static const int PgcuConvertTable[] = {
    16, AOHK_KEY_0,
    9, AOHK_KEY_1,
    10, AOHK_KEY_2,
    11, AOHK_KEY_3,
    5, AOHK_KEY_4,
    6, AOHK_KEY_5,
    7, AOHK_KEY_6,
    1, AOHK_KEY_7,
    2, AOHK_KEY_8,
    3, AOHK_KEY_9,
    12, AOHK_KEY_HASH,
    13, AOHK_KEY_HASH,

    15, AOHK_KEY_STAR,
    21, AOHK_KEY_STAR,

    17, AOHK_KEY_USR_1,
    18, AOHK_KEY_USR_2,
    19, AOHK_KEY_USR_3,
    20, AOHK_KEY_USR_4,
    4, AOHK_KEY_SPECIAL,

    // 14, 8, 22, 23. 24 unused

    22, AOHK_KEY_NOP,			// these produce only down, when switched up
    23, AOHK_KEY_NOP,
    24, AOHK_KEY_NOP,

    KEY_RESERVED, KEY_RESERVED
};

///
///	Input devices structure.
///	Contains pre-defined features for special products.
///
struct input_device
{
    const char *ID;			///< name to select this device
    int Vendor;				///< usb/bluetooth vendor id
    int Product;			///< usb/bluetooth product id
    const int *ConvertTable;		///< default input convert table
    int Offset;				///< to convert buttons of joysticks
};

///
///	Table of supported input devices.
///
static const struct input_device InputDevices[] = {
    {
	"n52", BELKIN_VENDOR_ID, NOSTROMO_N52_ID, N52ConvertTable, 0}, {
	"pc102", SYSTEM_VENDOR_ID, PS2_KEYBOARD_ID, PC102ConvertTable, 0}, {
	"keypad", CHERRY_VENDOR_ID, KEYPAD_ID, NumpadConvertTable, 0}, {
	"keypad", FUJSI_VENDOR_ID, KEYPAD2_ID, Numpad2ConvertTable, 0}, {
	"pgcu", SAITEK_VENDOR_ID, PGCU_ID, PgcuConvertTable, 0x11F}, {
	"awkb", APPLE_VENDOR_ID, AWKB_ID, PC102ConvertTable, 0}, {
	"aawkb", APPLE_VENDOR_ID, AAWKB_ID, PC102ConvertTable, 0}, {
	"aakb", APPLE_VENDOR_ID, AAKB_ID, PC102ConvertTable, 0}, {
	"g13", LOGITECH_VENDOR_ID, G13_ID, PC102ConvertTable, 0}, {
	NULL, 0, 0, NULL, 0}
};

#define MAX_INPUTS	32		///< max input devices supported
int InputFds[MAX_INPUTS];		///< inputs
int InputDid[MAX_INPUTS];		///< input device ID
int InputLEDs[MAX_INPUTS];		///< inputs LED support
int InputFdsN;				///< number of Inputs

int UInputFd;				///< output uinput file descriptor

int AOHKTimeout = 1000;			///< in: timeout used
int AOHKExit;				///< in: exit program flag

const char *UseDev;			///< wanted device name
int UseEvent = -1;			///< wanted event device number
int UseVendor = -1;			///< wanted vendor id
int UseProduct = -1;			///< wanted product id
int ListDevices;			///< show possible devices
int NoConvertTable;			///< don't load device convert table
int NoLed;				///< don't use Leds

int DebugLevel = 2;			///< debug level
int SysLog;				///< logging to syslog

//----------------------------------------------------------------------------
//	Debug / Logging
//----------------------------------------------------------------------------

///
///	Debug output function.
///
///	@param level	debug level (0: errors, 1: warnings, 2: infos: 3: ...)
///	@param fmt	printf like format string
///	@param ...	printf like arguments
///
#define Debug(level, fmt...) \
    do { if (level<DebugLevel) { printf(fmt); } } while (0)
///
///	Prepare debuging/logging.
///
#define InitDebug() \
    do { if (SysLog) { openlog("aohk-daemon", 0, 0); } } while (0)
///
///	Cleanup debuging/logging.
///
#define ExitDebug()

//----------------------------------------------------------------------------
//	Input event
//----------------------------------------------------------------------------

///
///	Check if device supports LEDs.
///
///	@param fd	file descriptor of input device
///
///	@returns True if device supports setting leds.
///
///	@note if #NoLed is set, this function returns always false.
///
static int EventCheckLEDs(int fd)
{
    unsigned char led_bitmask[LED_MAX / 8 + 1] = { 0 };

    // Look for a device to control LEDs on
    if (ioctl(fd, EVIOCGBIT(EV_LED, sizeof(led_bitmask)), led_bitmask) >= 0) {
	if (led_bitmask[0]) {
	    Debug(1, "%d: supports led\n", fd);
	    return NoLed ? 0 : 1;
	}
    }
    return 0;
}

///
///	Open input event device.
///
///	@returns number of opened of input devices, 0 on failure
///
static int OpenEvent(void)
{
    char dev[32];
    char name[64];
    char *s;
    int i;
    int j;
    int fd;
    struct input_id info;

    strcpy(dev, "/dev/input/event");
    s = dev + sizeof("/dev/input/event") - 1;
    s[1] = '\0';
    s[2] = '\0';
    for (i = 0; i < 32; ++i) {		// 0 - 31 supported
	if (i < 10) {
	    s[0] = '0' + i;
	} else {
	    s[0] = '0' + i / 10;
	    s[1] = '0' + i % 10;
	}
	// printf("Trying '%s'\n", dev);
	if ((fd = open(dev, O_RDWR)) >= 0) {
	    // Open ok
	    // printf("Open success\n");
	    if (!ioctl(fd, EVIOCGID, &info)) {
		if (ListDevices) {
		    // get device name if possible
		    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
			*name = '\0';
		    }
		    Debug(1,
			"Bus:%04X Vendor:%04X Product:%04X "
			"Version:%04X %s\n", info.bustype, info.vendor,
			info.product, info.version, name);
		}
		if (UseEvent == i || (UseVendor == info.vendor
			&& UseProduct == info.product)) {
		    Debug(1,
			"Device(%d) found BUS: %04X Vendor: %04X Product: "
			"%04X Version: %04X\n", fd, info.bustype, info.vendor,
			info.product, info.version);
		    // Grab the device for exclusive use
		    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
			perror("ioctl(EVIOCGRAB)");
		    }
		    if (InputFdsN < MAX_INPUTS) {
			InputDid[InputFdsN] = 0;
			InputLEDs[InputFdsN] = EventCheckLEDs(fd);
			InputFds[InputFdsN++] = fd;
		    }
		    goto found;
		}
		for (j = 0; InputDevices[j].ID; ++j) {
		    //
		    //	Try only the requested input device.
		    //
		    if (UseDev && strcasecmp(UseDev, InputDevices[j].ID)) {
			continue;
		    }
		    if (InputDevices[j].Vendor == info.vendor
			&& InputDevices[j].Product == info.product) {
			Debug(1,
			    "Device(%d) '%s' found BUS: %04X Vendor: %04X "
			    "Product: %04X Version: %04X\n", fd,
			    InputDevices[j].ID, info.bustype, info.vendor,
			    info.product, info.version);
			// Grab the device for exclusive use
			if (ioctl(fd, EVIOCGRAB, 1) < 0) {
			    perror("ioctl(EVIOCGRAB)");
			}
			if (InputFdsN < MAX_INPUTS) {
			    InputDid[InputFdsN] = j;
			    InputLEDs[InputFdsN] = EventCheckLEDs(fd);
			    InputFds[InputFdsN++] = fd;
			}
			if (!NoConvertTable) {
			    AOHKSetupConvertTable(InputDevices
				[j].ConvertTable);
			}
			goto found;
		    }
		}
	    } else {
		perror("ioctl(EVIOCGID)");
	    }
	    close(fd);
	} else if (errno != ENOENT) {
	    fprintf(stderr, "open(%s):%s\n", dev, strerror(errno));
	}
      found:;
    }

    if (!InputFdsN) {
	Debug(0, "No useable device found.\n");
    }

    return InputFdsN;
}

///
///	Turn LED on/off.
///
///	@param fd	file descriptor of input device
///	@param num	led integer number
///	@param state	true turn led on, false turn led off
///
///	@see LED_NUML, LED_CAPSL, LED_SCROLLL, ... in /usr/include/linux/input.h
///
static void EventLEDs(int fd, int num, int state)
{
    struct input_event ev;

    ev.type = EV_LED;
    ev.code = num;
    ev.value = state;
    if (write(fd, &ev, sizeof(ev)) < 0) {
	perror("write()");
    }
}

//----------------------------------------------------------------------------
//	Highlevel
//----------------------------------------------------------------------------

static int TouchFd = -1;		///< touchpad file descriptor
static int TouchX;			///< x of touchpad
static int TouchY;			///< y of touchpad
static int TouchP;			///< pressure of touchpad
static int TouchR;			///< tool width of touchpad
static int TouchB;			///< touch button

static int TouchXI;			///< invert x of touchpad
static int TouchX0 = 1350;		///< x0 of touchpad
static int TouchXC = 1350 + 140;	///< x corner of touchpad
static int TouchX1 = 1350 + 1415;	///< x1 of touchpad
static int TouchX2 = 1350 + 2830;	///< x2 of touchpad
static int TouchX3 = 5595;		///< x3 of touchpad

static int TouchYI;			///< invert y of touchpad
static int TouchY0 = 1200;		///< y0 of touchpad
static int TouchYC = 1200 + 400;	///< y corner of touchpad
static int TouchY1 = 1200 + 1167 - 100;	///< y1 of touchpad
static int TouchY2 = 1200 + 2333 + 100;	///< y2 of touchpad
static int TouchY3 = 4750;		///< y3 of touchpad

static int TouchLastSector;		///< last pressed sector

///
///	Show LED.
///
///	@param num	led integer number
///	@param state	true turn led on, false turn led off
///
///	@see LED_NUML, LED_CAPSL, LED_SCROLLL, ... in /usr/include/linux/input.h
///
void AOHKShowLED(int num, int state)
{
    int i;

    //	Search device for LEDs.
    for (i = 0; i < InputFdsN; ++i) {
	if (InputLEDs[i]) {
	    // printf("LED: %d\n", InputLEDs[i] );
	    EventLEDs(InputFds[i], num, state);
	}
    }
}

///
///	Input handle touchpad/touchscreen devices.
///
///	@param did	internal device id (#InputDevices) of input
///	@param fd	file descriptor of input device
///	@param ev	input event
///
///	@todo only debug
///
static void InputTouch(int did, int fd, const struct input_event *ev)
{
    int i;
    int sector;
    int timestamp;

    static int sector_codes[13] = { AOHK_KEY_0,
	AOHK_KEY_1, AOHK_KEY_2, AOHK_KEY_3,
	AOHK_KEY_4, AOHK_KEY_5, AOHK_KEY_6,
	AOHK_KEY_7, AOHK_KEY_8, AOHK_KEY_9,
	AOHK_KEY_HASH, AOHK_KEY_SPECIAL, AOHK_KEY_STAR
    };

    did = did;

    if (TouchFd == -1) {		// touchpad autodection
	if (ev->code == BTN_TOUCH || ev->code == ABS_PRESSURE
	    || ev->code == ABS_TOOL_WIDTH) {
	    Debug(5, "touch device auto detected\n");
	    TouchFd = fd;
	} else {
	    return;
	}
    } else if (fd != TouchFd) {
	return;
    }

    if (AOHKCheckOffState()) {		// turned off
	if (write(UInputFd, ev, sizeof(*ev)) != sizeof(*ev)) {
	    perror("write");
	}
	// FIXME: can't turn touchpad on again!
	return;
    }

    timestamp = ev->time.tv_sec * 1000 + ev->time.tv_usec / 1000;

    if (ev->type == EV_SYN) {
	//	Ignore out of range events
	if (TouchX0 > TouchX || TouchX > TouchX3 || TouchY0 > TouchY
	    || TouchY > TouchY3) {
	    Debug(3, "out of range\n");
	    return;
	}
	//
	//	Unhandled button event
	//
	if (TouchB) {
	    //	Convert to sector
	    i = TouchX;
	    if (i < TouchX0 || i > TouchX3) {
		if (TouchB < 0) {
		    if (TouchLastSector) {
			AOHKFeedSymbol(timestamp,
			    sector_codes[TouchLastSector], 0);
		    }
		    Debug(1, "x out of range\n");
		    TouchB = 0;
		}
		return;
	    }
	    if (i < TouchX2) {
		if (i < TouchX1) {
		    sector = 1;
		} else {
		    sector = 2;
		}
	    } else {
		sector = 3;
	    }
	    if (TouchXI) {
		sector = 3 - sector;
	    }
	    i = TouchY;
	    if (i < TouchY0 || i > TouchY3) {
		if (TouchB < 0) {
		    if (TouchLastSector) {
			AOHKFeedSymbol(timestamp,
			    sector_codes[TouchLastSector], 0);
		    }
		    Debug(1, "y out of range\n");
		    TouchB = 0;
		}
		return;
	    }
	    if (i < TouchY2) {
		if (i < TouchY1) {
		    sector += TouchYI ? 0 : 6;
		} else {
		    sector += 3;
		}
	    } else {
		sector += TouchYI ? 6 : 0;
	    }
	    TouchB++;			// to release code
	    if (sector == 7 && TouchX < TouchXC && TouchY < TouchYC) {
		Debug(3, "Corner %d %s\n", sector,
		    TouchB ? "press" : "release");
		sector = 11;
	    }
	    Debug(3, "Sector %d %s\n", sector, TouchB ? "press" : "release");
	    // release not the same sector release the old sector
	    if (TouchLastSector && TouchLastSector != sector) {
		// release old sector, press new sector
		AOHKFeedSymbol(timestamp, sector_codes[TouchLastSector], 0);
		if (!TouchB) {		// release in new sector
		    AOHKFeedSymbol(timestamp, sector_codes[sector], 1);
		}
	    }
	    AOHKFeedSymbol(timestamp, sector_codes[sector], TouchB);
	    if (TouchB) {		// remember to release
		TouchLastSector = sector;
	    } else {
		TouchLastSector = 0;
	    }
	    TouchB = 0;
	} else {
	    Debug(1, "%d x:%d,y:%d,p:%d,r:%d\n", TouchB, TouchX, TouchY,
		TouchP, TouchR);
	}
	return;
    }
    if (ev->type == EV_KEY) {
	if (ev->code == BTN_LEFT) {
	    Debug(1, "0 %s\n", ev->value ? "pressed" : "released");
	    AOHKFeedSymbol(timestamp, sector_codes[0], ev->value);
	    return;
	}
	if (ev->code == BTN_RIGHT) {
	    Debug(1, "# %s\n", ev->value ? "pressed" : "released");
	    AOHKFeedSymbol(timestamp, sector_codes[10], ev->value);
	    return;
	}
	if (ev->code == BTN_MIDDLE) {
	    Debug(1, "* %s\n", ev->value ? "pressed" : "released");
	    AOHKFeedSymbol(timestamp, sector_codes[12], ev->value);
	    return;
	}
	if (ev->code != BTN_TOUCH) {
	    return;
	}
	TouchB = ev->value ? 1 : -1;

	return;
    }

    switch (ev->code) {
	case ABS_X:
	    TouchX = ev->value;
	    break;
	case ABS_Y:
	    TouchY = ev->value;
	    break;
	case ABS_PRESSURE:
	    if (0) {
		if (TouchP && !ev->value) {
		    TouchB = -1;
		} else if (!TouchP && ev->value) {
		    TouchB = 1;
		}
		Debug(1, "pressure %d %d\n", TouchP, TouchB);
	    }
	    TouchP = ev->value;
	    break;
	case ABS_TOOL_WIDTH:
	    TouchR = ev->value;
	    break;
	default:
	    Debug(1, "unsupported event code $%02x\n", ev->code);
	    break;
    }
    Debug(9, "M x:%d,y:%d,p:%d,r:%d\n", TouchX, TouchY, TouchP, TouchR);
}

///
///	Input read.
///	Read input from events, emulate aohk, output to uinput.
///
///	@param did	internal device id (#InputDevices) of input
///	@param fd	file descriptor of input device
///
///	@see InputDevices
///
static void InputRead(int did, int fd)
{
    struct input_event ev;

    if (read(fd, &ev, sizeof(ev)) != sizeof(ev)) {
	perror("read()");
	return;
    }
    switch (ev.type) {
	case EV_KEY:			// Key event
	    if (ev.code >= BTN_MISC) {
		Debug(4, "Key 0x%02X=%d %s\n", ev.code, ev.code,
		    ev.value ? "pressed" : "released");
		InputTouch(did, fd, &ev);
		break;
	    }
	    // FIXME: 0x100 - 0x200
	    // Feed into AOHK Statemachine
	    AOHKFeedKey(ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000,
		ev.code - InputDevices[did].Offset, ev.value);
	    break;

	case EV_LED:			// Led event
	    // Ignore
	    break;

	case EV_MSC:			// Send by system keyboard
	    // Ignore
	    Debug(5, "msc for %d\n", fd);
	    break;
	case EV_SYN:			// Synchronization events
	    // Ignore
	    Debug(5, "syn for %d\n", fd);
	    InputTouch(did, fd, &ev);
	    break;
	case EV_ABS:			// ABS event parse on.
	    // ev.code: ABS_X, ABS_Y, ABS_PRESSURE, ABS_TOOLWIDTH
	    Debug(6, "Event Type(%d,$%02x,%d) abs\n", ev.type, ev.code,
		ev.value);
	    InputTouch(did, fd, &ev);
	    break;

	default:
	    Debug(1, "Event Type(%d,%d,%d) unsupported\n", ev.type, ev.code,
		ev.value);
	    if (write(UInputFd, &ev, sizeof(ev)) != sizeof(ev)) {
		perror("write");
	    }
	    break;
    }
}

///
///	Event Loop
///
void EventLoop(void)
{
    struct pollfd fds[MAX_INPUTS + 1];
    int ret;
    int i;

    for (i = 0; i < InputFdsN; ++i) {
	fds[i].fd = InputFds[i];
	fds[i].events = POLLIN;
	fds[i].revents = 0;
    }

    while (!AOHKExit) {
	ret = poll(fds, InputFdsN, AOHKTimeout ? AOHKTimeout : -1);
	if (ret < 0) {			// -1 error
	    perror("poll()");
	    continue;
	}
	if (!ret) {
	    // FIXME: not correct. this can be longer
	    AOHKFeedTimeout(AOHKTimeout);
	    continue;
	}
	for (i = 0; i < InputFdsN; ++i) {
	    if (fds[i].revents) {
		InputRead(InputDid[i], fds[i].fd);
		fds[i].revents = 0;
	    }
	}
    }
}

///
///	@details Key out, called from aohk module to output final scancodes.
///
///	@param key	linux scan code for key (/usr/include/linux/input.h)
///	@param pressed	true key is pressed, false released.
///
void AOHKKeyOut(int key, int pressed)
{
    if (pressed) {
	UInputKeydown(UInputFd, key);
    } else {
	UInputKeyup(UInputFd, key);
    }
}

///
///	Show firework. No time lost, need some delay to release start keys.
///
static void Firework(void)
{
    static const int8_t led_firework[][3] = {
	{0, 0, 0},
	{1, 0, 0},
	{0, 1, 0},
	{0, 0, 1},
	{0, 1, 0},
	{1, 0, 0},
	{0, 1, 0},
	{0, 0, 1},
	{0, 1, 0},
	{1, 0, 0},
	{0, 1, 0},
	{0, 0, 1},
	{0, 0, 0},
	{-1, -1, -1},
    };
    int i;

    for (i = 0; led_firework[i][0] >= 0; ++i) {
	AOHKShowLED(0, led_firework[i][0]);
	AOHKShowLED(1, led_firework[i][1]);
	AOHKShowLED(2, led_firework[i][2]);
	usleep(100000);
    }
}

///
///	Parse geometry.
///	Parses strings of the form
///	"=<width>x<height>{+-}<xoffset>{+-}<yoffset>", where
///	width, height, xoffset, and yoffset are unsigned integers.
///
///	@param string		geometry (f.e. 5000x5000+1300+1300)
///	@param[out] x		pointer for x offset result
///	@param[out] y		pointer for y offset result
///	@param[out] width	pointer for width result
///	@param[out] height	pointer for height result
///
///	@returns 0 on parse errrors, otherwise bitmask of results.
///
static int ParseGeometry(const char *string, int *x, int *y, unsigned *width,
    unsigned *height)
{
    int mask;
    char *endptr;
    int tx;
    int ty;
    unsigned tw;
    unsigned th;

    mask = 0;
    if (!string || !*string) {		// empty or null string
	return mask;
    }
    if (*string == '=') {		// ignore leading '='
	++string;
    }
    if (*string != '+' && *string != '-' && (*string | 0x20) != 'x') {
	tw = strtol(string, &endptr, 10);
	if (string == endptr) {		// parse failure
	    return 0;
	}
	string = endptr;
	mask |= 4;
    }
    if ((*string | 0x20) == 'x') {
	th = strtol(++string, &endptr, 10);
	if (string == endptr) {		// parse failure
	    return 0;
	}
	string = endptr;
	mask |= 8;
    }
    if (*string == '+' || *string == '-') {
	if (*string == '-') {
	    mask |= 16;
	}
	tx = strtol(++string, &endptr, 10);
	if (mask & 16) {
	    tx = -tx;
	}
	if (string == endptr) {		// parse failure
	    return 0;
	}
	string = endptr;
	mask |= 1;
	if (*string == '+' || *string == '-') {
	    if (*string == '-') {
		mask |= 32;
	    }
	    ty = strtol(++string, &endptr, 10);
	    if (mask & 32) {
		ty = -ty;
	    }
	    if (string == endptr) {	// parse failure
		return 0;
	    }
	    string = endptr;
	    mask |= 2;
	}
    }

    if (*string) {			// still some characters left invalid geometry
	return 0;
    }
    if (mask & 1) {			// everything ok, give results back
	*x = tx;
    }
    if (mask & 2) {
	*y = ty;
    }
    if (mask & 3) {
	*width = tw;
    }
    if (mask & 4) {
	*height = th;
    }
    return mask;
}

///
///	List supported devices
///
static void ListSupportedDevices(void)
{
    int j;

    for (j = 0; InputDevices[j].ID; ++j) {
	if (UseDev && strcasecmp(UseDev, InputDevices[j].ID)) {
	    printf("*");
	}
	printf("%s ", InputDevices[j].ID);
    }
}

    /// Title shown for errors, usage.
#define TITLE	"ALE one-hand keyboard daemon Version " VERSION \
	" (c) 2007,2009 Lutz Sammer"

///
///	Main entry point.
///
///	@param argc	Number of arguments
///	@param argv	Arguments vector
///
///	@returns -1 on failures
///
int main(int argc, char *const *argv)
{
    int i;
    int ufd;
    int background;
    const char *save;
    const char *lang;

    lang = "de";			// My choice :>
    background = 0;
    save = NULL;
    SysLog = 0;

    //
    //	Parse arguments:
    //		-l loading keyboard mapping.
    //		-d wich device
    //		...
    //
    for (;;) {
	switch (getopt(argc, argv, "DLQ:bd:e:g:l:np:s:v:h?-")) {
	    case 'b':			// background
		background = 1;
		SysLog = 1;
		continue;
	    case 'd':			// device
		UseDev = optarg;
		continue;
	    case 'e':			// event device
		UseEvent = strtol(optarg, NULL, 0);
		continue;
	    case 'g':			// geometry
	    {
		int m;
		int x;
		int y;
		unsigned w;
		unsigned h;

		m = ParseGeometry(optarg, &x, &y, &w, &h);
		printf("%x = %dx%d%+d%+d\n", m, w, h, x, y);

		if (m & 16) {
		    TouchXI = 1;	// negative invert
		    x = -x;
		}
		TouchX0 = x;
		TouchXC = x + 140;
		TouchX1 = x + (w / 3);
		TouchX2 = x + 2 * (w / 3);
		TouchX3 = x + w;
		if (m & 32) {
		    TouchYI = 1;	// negative invert
		    y = -y;
		}
		TouchY0 = y;
		TouchYC = y + 400;
		TouchY1 = y + (h / 3);
		TouchY2 = y + 2 * (h / 3);
		TouchY3 = y + h;

	    }
		continue;
	    case 'l':			// language
		if (strlen(optarg) != 2) {
		    fprintf(stderr,
			"%s\nUse 2 character language codes. FE. 'de', 'us'\n",
			TITLE);
		    return -1;
		}
		lang = optarg;
		continue;
	    case 'n':			// no leds
		NoLed = 1;
		continue;
	    case 'p':			// product id
		UseProduct = strtol(optarg, NULL, 0);
		continue;
	    case 's':			// save internal tables
		save = optarg;
		continue;
	    case 'v':			// vendor id
		UseVendor = strtol(optarg, NULL, 0);
		continue;
	    case 'D':			// debug
		DebugLevel++;
		continue;
	    case 'Q':			// quiet
		if (DebugLevel) {
		    DebugLevel--;
		}
		continue;
	    case 'L':			// list
		ListDevices = 1;
		continue;;

	    case EOF:
		break;
	    case '-':			// stupid long opts
		fprintf(stderr, "long options not needed\n");
		// fall through
	    case '?':
	    case 'h':			// help usage
		printf("%s\nUsage: %s [OPTIONs]... [FILEs]...\t"
		    "load specified file(s)\n"
		    "	or: %s [OPTIONs]... -\tread mapping from stdin\n"
		    "Options:\n" "-h\tPrint this page\n"
		    "-b\tBackground, run as daemon\n"
		    "-L\tList all available input devices\n"
		    "-D\tIncrease debug level\n"
		    "-d dev\tUse only this input device\n"
		    "-e n\tAlso use this /dev/input/eventN device\n"
		    "-v id\tAlso use the input device with vendor id\n"
		    "-p id\tAlso use the input device with product id\n"
		    "-g geo\tGeometry of the touch device <width>x<height>{+-}<xoffset>{+-}<yoffset\n"
		    "-n\tNo leds, some control goes wired with leds\n"
		    "-l lang\tUse internal language table (de,us)\n"
		    "-s file\tSave internal tables\nSupported input devices: ",
		    TITLE, argv[0], argv[0]);
		ListSupportedDevices();
		printf("\n");
		return 0;
	    case ':':
		fprintf(stderr, "%s\nMissing argument for option '%c'\n",
		    TITLE, optopt);
		return -1;
	    default:
		fprintf(stderr, "%s\nUnkown option '%c'\n", TITLE, optopt);
		return -1;
	}
	break;
    }

    //
    //	Background
    //
    if (background) {
	if (daemon(0, 0)) {
	    perror(argv[0]);
	    return -1;
	}
    }

    InitDebug();
    Debug(0, "%s\n", TITLE);

    //
    //	Load language defaults.
    //
    AOHKSetLanguage(lang);

    //
    //	Remaining files load as keymap.
    //
    for (i = optind; i < argc; ++i) {
	NoConvertTable = 1;
	AOHKLoadTable(argv[i]);
    }

    if (save) {				// save resulting tables and exit
	AOHKSaveTable(save);
	return -1;
    }
    //
    //	Open input devices
    //
    if (!OpenEvent()) {
	return -1;
    }
    //
    //	Open output device
    //
    ufd = OpenUInput("ALE OneHand Keyboard");
    if (ufd >= 0) {
	UInputFd = ufd;
	if (!background && !SysLog) {
	    printf("Press SPECIAL 5 to exit\n");
	}
	//sleep(1);			// sleep 1s for key release
	Firework();
	EventLoop();

	CloseUInput(ufd);
    }
    //
    //	Close input devices, cleanup
    //
    for (i = 0; i < InputFdsN; ++i) {
	close(InputFds[i]);
    }

    ExitDebug();

    return 0;
}

/// @}
