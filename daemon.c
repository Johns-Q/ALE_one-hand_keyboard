///
///	@file daemon.c	@brief	Keyboard handler daemon.
///
///	Copyright (c) 2007,2009 by Lutz Sammer.  All Rights Reserved.
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

#define SYSTEM_VENDOR_ID	0x0001	///< System vendor
#define PS2_KEYBOARD_ID		0x0001	///< System keyboard

#define BELKIN_VENDOR_ID	0x050D	///< Belkin's vendor ID
#define NOSTROMO_N52_ID		0x0815	///< nostromo n52 USB ID

#define CHERRY_VENDOR_ID	0x046A	///< Cherry's vendor ID
#define KEYPAD_ID		0x0014	///< Keypad G84-4700 USB ID

#define FUJSI_VENDOR_ID		0x0BF8	///< Fujitsu Siemens vendor ID
#define KEYPAD2_ID		0x1004	///< Keypad USB ID

#define SAITEK_VENDOR_ID	0x06A3	///< Saitek's vendor ID
#define PGCU_ID			0x80C0	///< Pro Gamer Command Unit USB ID

#define APPLE_VENDOR_ID		0x05AC	///< Apple vendor ID
#define AWKB_ID			0x0209	///< Wireless keyboard BT ID
#define AAKB_ID			0x021E	///< Alu keyboard USB ID
#define AAWKB_ID		0x022D	///< Alu Wireless keyboard BT ID

#define	LOGITECH_VENDOR_ID	0x046D	///< Logitech vendoor ID
#define G13_ID			0xC21C	///< G13 USB keyboard ID

///
///	Mapping from keys to internal key for nostromo N52
///	End marked with KEY_RESERVED
///
static int N52ConvertTable[] = {
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
///	typing on the left side of keyboard with left hand.
///	End marked with KEY_RESERVED
///
static int PC102ConvertTable[] = {
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
///	typing with a number/key pad.
///	End marked with KEY_RESERVED
///
static int NumpadConvertTable[] = {
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
///	typing with a number/key pad.
///	End marked with KEY_RESERVED
///
static int Numpad2ConvertTable[] = {
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

//
//	Mapping from keys to internal key for Pro Gamer Command Unit
//	End marked with KEY_RESERVED
//
static int PgcuConvertTable[] = {
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
///	Table of supported input devices
///
struct input_device
{
    const char *ID;			///< name to select this device
    int Vendor;				///< usb/bluetooth vendor id
    int Product;			///< usb/bluetooth product id
    int *ConvertTable;			///< default input convert table
    int Offset;				///< to convert buttons of joysticks
} InputDevices[] = {
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
int InputFds[MAX_INPUTS];		///< Inputs
int InputDid[MAX_INPUTS];		///< Input device ID
int InputLEDs[MAX_INPUTS];		///< Inputs LED support
int InputFdsN;				///< Number of Inputs

int UInputFd;				///< Output

int Timeout = 1000;			///< in: timeout used
int Exit;				///< in: exit program flag

const char *UseDev;			///< Wanted device
int UseVendor;				///< Wanted vendor
int UseProduct;				///< Wanted product
int ListDevices;			///< Show possible devices
int NoConvertTable;			///< Don't load device convert table
int NoLed;				///< Don't use Leds

int DebugLevel = 2;			///< Debug level
int SysLog;				///< logging to syslog

//----------------------------------------------------------------------------
//	Debug / Logging
//----------------------------------------------------------------------------

///
///	Debug output function.
///
#define Debug(level, fmt...) \
    do { if (level<DebugLevel) { printf(fmt); } } while (0)

#define InitDebug() \
    do { if (SysLog) { openlog("aohk-daemon", 0, 0); } } while (0)
#define ExitDebug()

//----------------------------------------------------------------------------
//	Input event
//----------------------------------------------------------------------------

///
///	Check if device supports LEDs.
///
///	@param fd	File descriptor of input device
///
///	@returns True if device supports setting leds.
///
///	@note if #NoLeds is set, this function returns always false.
///
int EventCheckLEDs(int fd)
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
int OpenEvent(void)
{
    char dev[32];
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
	    if (ioctl(fd, EVIOCGID, &info) == 0) {
		if (ListDevices) {
		    Debug(1,
			"Bus: %04X Vendor: %04X Product: %04X "
			"Version: %04X\n", info.bustype, info.vendor,
			info.product, info.version);
		}
		if (UseVendor == info.vendor && UseProduct == info.product) {
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
			    AOHKSetupConvertTable(InputDevices[j].
				ConvertTable);
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
	return 1;
    }

    return 0;
}

///
///	Turn LED on/off.
///
void EventLEDs(int fd, int num, int state)
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
//	Uinput
//----------------------------------------------------------------------------

///
///	Open uinput device
///
int OpenUInput(void)
{
    int i;
    int fd;
    struct uinput_user_dev device;

    //	open uinput device file
    if ((fd = open("/dev/misc/uinput", O_RDWR)) < 0
	&& (fd = open("/dev/input/uinput", O_RDWR)) < 0
	&& (fd = open("/dev/uinput", O_RDWR)) < 0) {
	perror("open()");
	return fd;
    }

    memset(&device, 0, sizeof(device));

    //	sets the name of our device
    strncpy(device.name, "ALE OneHand Keyboard", UINPUT_MAX_NAME_SIZE);

    //	its bus
    device.id.bustype = BUS_USB;

    //	and vendor id/product id/version
    device.id.vendor = 0x414C;
    device.id.product = 0x4F48;
    device.id.version = 0x0001;

    device.absmin[ABS_X] = 0;
    device.absmin[ABS_Y] = 0;
    device.absmax[ABS_X] = 1023;
    device.absmax[ABS_Y] = 1023;

    device.absfuzz[ABS_X] = 4;
    device.absfuzz[ABS_Y] = 4;
    device.absflat[ABS_X] = 2;
    device.absflat[ABS_Y] = 2;

    //	mouse emulation
    //
    //	inform that we'll generate relative axis events
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    //ioctl(fd, UI_SET_EVBIT, EV_SYN);
    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);

#if 0
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
#endif

    //	inform that we'll generate key events
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    //	set key events we can generate (in this case, all)
    for (i = 1; i < /*KEY_MAX */ 255; i++) {
	ioctl(fd, UI_SET_KEYBIT, i);
    }

    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);

    //	write down information for creating a new device
    if (write(fd, &device, sizeof(struct uinput_user_dev)) < 0) {
	perror("write");
	close(fd);
	return -1;
    }
    //	actually creates the device
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
	perror("ioctl(UI_DEV_CREATE)");
	close(fd);
	return -1;
    }

    return fd;
}

///
///	Send keydown event
///
int UInputKeydown(int fd, int code)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = 1;

    return write(fd, &event, sizeof(event));
}

///
///	Send keyup event
///
int UInputKeyup(int fd, int code)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = 0;

    return write(fd, &event, sizeof(event));
}

///
///	Close UInput
///
void CloseUInput(int fd)
{
    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
	perror("ioctl(UI_DEV_DESTROY)");
    }
}

//----------------------------------------------------------------------------
//	Highlevel
//----------------------------------------------------------------------------

///
///	Show LED.
///
void ShowLED(int num, int state)
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
///	Input read
///
void InputRead(int did, int fd)
{
    struct input_event ev;

    if (read(fd, &ev, sizeof(ev)) != sizeof(ev)) {
	perror("read()");
	return;
    }
    switch (ev.type) {
	case EV_KEY:			// Key event
	    //printf("Key 0x%02X=%d %s\n", ev.code, ev.code,
	    //	ev.value ? "pressed" : "released");
	    // FIXME: 0x100 - 0x200
	    // Feed into AOHK Statemachine
	    AOHKFeedKey(ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000,
		ev.code - InputDevices[did].Offset, ev.value);
	    break;

	case EV_LED:			// Led event
	    // Ignore
	    break;

	case EV_MSC:			// Send by system keyboard
	case EV_SYN:			// Synchronization events
	    // Ignore
	    break;
	case EV_ABS:			// ABS event parse on.
	    Debug(1, "Event Type(%d,%d,%d) abs\n", ev.type, ev.code, ev.value);
#if 0
	    ev.type = EV_REL;
	    ev.code -= 512;
	    if (write(UInputFd, &ev, sizeof(ev)) != sizeof(ev)) {
		perror("write");
	    }
#endif
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

    while (!Exit) {
	ret = poll(fds, InputFdsN, Timeout ? Timeout : -1);
	if (ret < 0) {			// -1 error
	    perror("poll()");
	    continue;
	}
	if (!ret) {
	    AOHKFeedTimeout(Timeout);
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
///	Key out
///
void KeyOut(int key, int pressed)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = key;
    event.value = pressed;

    if (write(UInputFd, &event, sizeof(event)) != sizeof(event)) {
	perror("write");
    }
}

///
///	Show firework. No time lost, need some delay to release start keys.
///
void Firework(void)
{
    static char led_firework[][3] = {
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
	ShowLED(0, led_firework[i][0]);
	ShowLED(1, led_firework[i][1]);
	ShowLED(2, led_firework[i][2]);
	usleep(100000);
    }
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

#define VERSION	"ALE one-hand keyboard daemon Version 0.06"

///
///	Main entry point.
///
int main(int argc, char **argv)
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
	switch (getopt(argc, argv, "DLQ:bd:l:np:s:v:h?")) {
	    case EOF:
		break;
	    case 'b':			// background
		background = 1;
		SysLog = 1;
		continue;
	    case 'd':			// device
		UseDev = optarg;
		continue;
	    case 'l':			// language
		if (strlen(optarg) != 2) {
		    fprintf(stderr,
			"%s\nUse 2 character language codes. FE. 'de', 'us'\n",
			VERSION);
		    exit(-1);
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
		    "-v id\tAlso use the input device with vendor id\n"
		    "-p id\tAlso use the input device with product id\n"
		    "-n\tNo leds, some control goes wired with leds\n"
		    "-l lang\tUse internal language table (de,us)\n"
		    "-s file\tSave internal tables\nSupported input devices: ",
		    VERSION, argv[0], argv[0]);
		ListSupportedDevices();
		printf("\n");
		exit(0);
	    case ':':
		fprintf(stderr, "%s\nMissing argument for option '%c'\n",
		    VERSION, optopt);
		exit(-1);
	    default:
		fprintf(stderr, "%s\nUnkown option '%c'\n", VERSION, optopt);
		exit(-1);
	}
	break;
    }

    //
    //	Background
    //
    if (background) {
	if (daemon(0, 0)) {
	    perror(argv[0]);
	    exit(-1);
	}
    }

    InitDebug();
    Debug(0, "%s\n", VERSION);

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
	exit(-1);
    }
    //
    //	Open input devices
    //
    sleep(1);				// sleep 1s for key release
    if (OpenEvent()) {
	exit(-1);
    }
    //
    //	Open output device
    //
    ufd = OpenUInput();
    if (ufd >= 0) {
	UInputFd = ufd;
	if (!background && !SysLog) {
	    printf("Press SPECIAL 5 to exit\n");
	}
	Firework();
	EventLoop();

	CloseUInput(ufd);
    }
    // FIXME: Key relase got as event
    sleep(1);				// sleep 1s for key release

    //
    //	Close input devices, cleanup
    //
    for (i = 0; i < InputFdsN; ++i) {
	close(InputFds[i]);
    }

    ExitDebug();

    return 0;
}
