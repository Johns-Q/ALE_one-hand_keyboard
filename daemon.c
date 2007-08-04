//

/**@name daemon.c	-	Keyboard handler daemon.	*/
//
//      Copyright (c) 2007 by Lutz Sammer.  All Rights Reserved.
//
//      Contributor(s):
//
//      This file is part of ALE one-hand keyboard
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      $Id$
////////////////////////////////////////////////////////////////////////////

#include <linux/input.h>
#include <linux/uinput.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <poll.h>

#include "aohk.h"

#define SYSTEM_VENDOR_ID	0x0001	// System vendor
#define PS2_KEYBOARD_ID		0x0001	// System keyboard

#define BELKIN_VENDOR_ID	0x050D	// Belkin's vendor ID
#define NOSTROMO_N52_ID		0x0815	// n52 USB ID

#define CHERRY_VENDOR_ID	0x046A	// Cherry's vendor ID
#define KEYPAD_ID		0x0014	// Keypad G84-4700 USB ID

//
//      Mapping from keys to internal key for nostromo N52
//      End marked with KEY_RESERVED
//
static int N52ConvertTable[] = {
    KEY_SPACE, OH_QUOTE,
    KEY_Z, OH_KEY_1,
    KEY_X, OH_KEY_2,
    KEY_C, OH_KEY_3,
    KEY_A, OH_KEY_4,
    KEY_S, OH_KEY_5,
    KEY_D, OH_KEY_6,
    KEY_Q, OH_KEY_7,
    KEY_W, OH_KEY_8,
    KEY_E, OH_KEY_9,
    KEY_LEFTSHIFT, OH_REPEAT,

    KEY_LEFTALT, OH_MACRO,

    KEY_TAB, OH_USR_1,
    KEY_CAPSLOCK, OH_USR_2,
    KEY_R, OH_USR_3,
    //KEY_F, OH_USR_4,
    //KEY_LEFT, OH_USR_5,
    //KEY_UP, OH_USR_6,
    //KEY_RIGHT, OH_USR_7,
    //KEY_DOWN, OH_USR_8,
    KEY_F, OH_SPECIAL,
    KEY_RESERVED, KEY_RESERVED
};

//
//      Mapping from keys to internal key for normal keyboard.
//      typing on the left side of keyboard with left hand.
//      End marked with KEY_RESERVED
//
static int PC102ConvertTable[] = {
    KEY_SPACE, OH_QUOTE,
    KEY_Z, OH_KEY_1,
    KEY_X, OH_KEY_2,
    KEY_C, OH_KEY_3,
    KEY_A, OH_KEY_4,
    KEY_S, OH_KEY_5,
    KEY_D, OH_KEY_6,
    KEY_Q, OH_KEY_7,
    KEY_W, OH_KEY_8,
    KEY_E, OH_KEY_9,
    KEY_LEFTSHIFT, OH_REPEAT,

    KEY_LEFTALT, OH_MACRO,

    KEY_TAB, OH_USR_1,
    KEY_1, OH_USR_2,
    KEY_2, OH_USR_3,
    KEY_3, OH_USR_4,
    KEY_4, OH_USR_5,
    KEY_R, OH_USR_6,
    KEY_F, OH_USR_7,
    KEY_V, OH_USR_8,
    KEY_GRAVE, OH_SPECIAL,
    KEY_RESERVED, KEY_RESERVED
};

//
//      Mapping from keys to internal key for normal keyboard.
//      typing with a number/key pad.
//      End marked with KEY_RESERVED
//
static int NumpadConvertTable[] = {
    KEY_KPENTER, OH_QUOTE,
    KEY_KP1, OH_KEY_1,
    KEY_KP2, OH_KEY_2,
    KEY_KP3, OH_KEY_3,
    KEY_KP4, OH_KEY_4,
    KEY_KP5, OH_KEY_5,
    KEY_KP6, OH_KEY_6,
    KEY_KP7, OH_KEY_7,
    KEY_KP8, OH_KEY_8,
    KEY_KP9, OH_KEY_9,
    KEY_KP0, OH_REPEAT,

    KEY_KPDOT, OH_MACRO,
    KEY_KPCOMMA, OH_MACRO,

    KEY_KPSLASH, OH_USR_1,
    KEY_KPASTERISK, OH_USR_2,
    KEY_KPMINUS, OH_USR_3,
    KEY_KPPLUS, OH_USR_4,
    KEY_ESC, OH_USR_5,
    KEY_LEFTCTRL, OH_USR_6,
    KEY_LEFTALT, OH_USR_7,
    KEY_BACKSPACE, OH_USR_8,
    KEY_NUMLOCK, OH_SPECIAL,
    KEY_RESERVED, KEY_RESERVED
};

//
//      Table of supported input devices/
struct input_device
{
    char *ID;
    int Vendor;
    int Product;
    int *ConvertTable;
} InputDevices[] = {
    {
    "n52", BELKIN_VENDOR_ID, NOSTROMO_N52_ID, N52ConvertTable}, {
    "pc102", SYSTEM_VENDOR_ID, PS2_KEYBOARD_ID, PC102ConvertTable}, {
    "keypad", CHERRY_VENDOR_ID, KEYPAD_ID, NumpadConvertTable}, {
    NULL, 0, 0, NULL}
};

#define MAX_INPUTS	32
int InputFds[MAX_INPUTS];		// Inputs
int InputLEDs[MAX_INPUTS];		// Inputs LED support
int InputFdsN;				// Number of Inputs

int UInputFd;				// Output

int Timeout = 1000;			// in: timeout used
int Exit;				// in: exit

const char *UseDev;			// Wanted device
int UseVendor;				// Wanted vendor
int UseProduct;				// Wanted product
int ListDevices;			// Show possible devices
int NoConvertTable;			// Don't load device convert table

//----------------------------------------------------------------------------
//      Input event
//----------------------------------------------------------------------------

//
//      Check if device supports LEDs.
//
int EventCheckLEDs(int fd)
{
    unsigned char led_bitmask[LED_MAX / 8 + 1] = { 0 };

    // Look for a device to control LEDs on 
    if (ioctl(fd, EVIOCGBIT(EV_LED, sizeof(led_bitmask)), led_bitmask) >= 0) {
	if (led_bitmask[0]) {
	    printf("%d: supports led\n", fd);
	    return 1;
	}
    }
    return 0;
}

//
//      Open input event device.
//
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
		    printf("Bus: %04X Vendor: %04X Product: %04X "
			"Version: %04X\n", info.bustype, info.vendor,
			info.product, info.version);
		}
		if (UseVendor == info.vendor && UseProduct == info.product) {
		    printf("Device found BUS: %04X Vendor: %04X Product: "
			"%04X Version: %04X\n", info.bustype, info.vendor,
			info.product, info.version);
		    // Grab the device for exclusive use
		    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
			perror("ioctl(EVIOCGRAB)");
		    }
		    if (InputFdsN < MAX_INPUTS) {
			InputLEDs[InputFdsN] = EventCheckLEDs(fd);
			InputFds[InputFdsN++] = fd;
		    }
		    goto found;
		}
		for (j = 0; InputDevices[j].ID; ++j) {
		    //
		    //  Try only the requested input device.
		    //
		    if (UseDev && strcasecmp(UseDev, InputDevices[j].ID)) {
			continue;
		    }
		    if (InputDevices[j].Vendor == info.vendor
			&& InputDevices[j].Product == info.product) {
			printf("Device '%s' found BUS: %04X Vendor: %04X "
			    "Product: %04X Version: %04X\n",
			    InputDevices[j].ID, info.bustype, info.vendor,
			    info.product, info.version);
			// Grab the device for exclusive use
			if (ioctl(fd, EVIOCGRAB, 1) < 0) {
			    perror("ioctl(EVIOCGRAB)");
			}
			if (InputFdsN < MAX_INPUTS) {
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
	} else {
	    // perror("open()");
	}
      found:;
    }

    if (!InputFdsN) {
	printf("No useable device found.\n");
	return 1;
    }

    return 0;
}

//
//      Turn LED on/off.
//
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
//      Uinput
//----------------------------------------------------------------------------

//
//      Open uinput device
//
int OpenUInput(void)
{
    int i;
    int fd;
    struct uinput_user_dev device;

    //  open uinput device file
    if ((fd = open("/dev/misc/uinput", O_RDWR)) < 0
	&& (fd = open("/dev/input/uinput", O_RDWR)) < 0
	&& (fd = open("/dev/uinput", O_RDWR)) < 0) {
	perror("open()");
	return fd;
    }

    memset(&device, 0, sizeof(device));

    //  sets the name of our device
    strncpy(device.name, "ALE OneHand Keyboard", UINPUT_MAX_NAME_SIZE);

    //  its bus
    device.id.bustype = BUS_USB;

    //  and vendor id/product id/version
    device.id.vendor = 0x414C;
    device.id.product = 0x4F48;
    device.id.version = 0x0001;

    //  inform that we'll generate key events
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    //  set key events we can generate (in this case, all)
    for (i = 1; i < /*KEY_MAX */ 255; i++) {
	ioctl(fd, UI_SET_KEYBIT, i);
    }

    //  write down information for creating a new device
    if (write(fd, &device, sizeof(struct uinput_user_dev)) < 0) {
	perror("write");
	close(fd);
	return -1;
    }
    //  actually creates the device
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
	perror("ioctl(UI_DEV_CREATE)");
	close(fd);
	return -1;
    }

    return fd;
}

//
//      Send keydown event
//
int UInputKeydown(int fd, int code)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = 1;

    return write(fd, &event, sizeof(event));
}

//
//      Send keyup event
//
int UInputKeyup(int fd, int code)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = 0;

    return write(fd, &event, sizeof(event));
}

//
//      Close UInput
//
void CloseUInput(int fd)
{
    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
	perror("ioctl(UI_DEV_DESTROY)");
    }
}

//----------------------------------------------------------------------------
//      Highlevel
//----------------------------------------------------------------------------

//
//      Show LED.
//
void ShowLED(int num, int state)
{
    int i;

    //  Search device for LEDs.
    for (i = 0; i < InputFdsN; ++i) {
	if (InputLEDs[i]) {
	    // printf("LED: %d\n", InputLEDs[i] );
	    EventLEDs(InputFds[i], num, state);
	}
    }
}

//
//      Input read
//
void InputRead(int fd)
{
    struct input_event ev;

    if (read(fd, &ev, sizeof(ev)) != sizeof(ev)) {
	perror("read()");
	return;
    }
    switch (ev.type) {
	case EV_KEY:			// Key event
	    // printf("Key %d %s\n", ev.code, ev.value ? "pressed" : "released");
	    // Feed into AOHK Statemachine
	    AOHKFeedKey(ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000,
		ev.code, ev.value);
	    break;

	case EV_LED:			// Led event
	    // Ignore
	    break;

	case EV_MSC:			// Send by system keyboard
	case EV_SYN:			// Synchronization events
	    // Ignore
	    break;
	default:
	    printf("Event Type(%d,%d,%d) unsupported\n", ev.type, ev.code,
		ev.value);
	    break;
    }
}

//
//      Event Loop
//
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
		InputRead(fds[i].fd);
		fds[i].revents = 0;
	    }
	}
    }
}

//
//      Key out
//
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

//
//      Show firework.
//
void Firework(void)
{
    static char led_firework[][3] = {
	{0, 0, 0},
	{1, 0, 0},
	{0, 1, 0},
	{0, 0, 1},
	{0, 1, 0},
	{1, 0, 0},
	{0, 0, 0},
	{1, 1, 1},
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

//
//      List supported devices
//
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

//
//      Main entry point.
//
int main(int argc, char **argv)
{
    int i;
    int ufd;
    int debugtable;
    const char *lang;

    lang = "de";			// My choice :>
    debugtable = 0;

    printf("ALE one-hand keyboard daemon Version 0.03\n");
    //
    //  Arguments:
    //          -l loading keyboard mapping.
    //          -d wich device
    //
    for (;;) {
	switch (getopt(argc, argv, "DLl:d:p:s:v:h?")) {
	    case EOF:
		break;
	    case 'd':			// device
		UseDev = optarg;
		continue;
	    case 'l':			// language
		if (strlen(optarg) != 2) {
		    printf("Use 2 character language codes. FE. 'de', 'us'\n");
		    exit(-1);
		}
		lang = optarg;
		continue;
	    case 'p':			// product id
		UseProduct = strtol(optarg, NULL, 0);
		continue;
	    case 's':
		// FIXME: hack
		AOHKSetupConvertTable(InputDevices[0].ConvertTable);
		AOHKSetLanguage(lang);
		AOHKSaveTable(optarg);
		exit(-1);
	    case 'v':			// vendor id
		UseVendor = strtol(optarg, NULL, 0);
		continue;
	    case 'D':
		debugtable = 1;
		continue;
	    case 'L':			// list
		ListDevices = 1;
		continue;;

	    case 'h':
		printf("Usage: %s [OPTIONs]... [FILEs]...\t"
		    "load specified file(s)\n"
		    "   or: %s [OPTIONs]... -\t\tread mapping from stdin\n"
		    "Options:\n" "-h\tPrint this page\n"
		    "-L\tList all available input devices\n"
		    "-d dev\tUse only this input device\n"
		    "-v id\tAlso use the input device with vendor id\n"
		    "-p id\tAlso use the input device with product id\n"
		    "-s file\tSave internal tables\n", argv[0], argv[0]);
		printf("Supported Inputdevices: ");
		ListSupportedDevices();
		printf("\n");
		exit(0);
	    case ':':
		printf("Missing argument for option '%c'\n", optopt);
		exit(-1);
	    default:
		printf("Unkown option '%c'\n", optopt);
		exit(-1);
	}
	break;
    }

    //
    //  Load language defaults.
    //
    AOHKSetLanguage(lang);

    //
    //  Remaining files load as keymap.
    //
    for (i = optind; i < argc; ++i) {
	NoConvertTable = 1;
	AOHKLoadTable(argv[i]);
    }

    if (OpenEvent()) {
	exit(-1);
    }
    //
    //  For debug save current mapping.
    //
    if (debugtable) {
	AOHKSaveTable("debug.map");
    }

    ufd = OpenUInput();
    if (ufd) {
	UInputFd = ufd;
	printf("Press SPECIAL 5 to exit\n");
	Firework();
	EventLoop();

	CloseUInput(ufd);
    }
    for (i = 0; i < InputFdsN; ++i) {
	close(InputFds[i]);
    }

    return 0;
}
