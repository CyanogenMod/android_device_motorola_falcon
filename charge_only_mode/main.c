/**************************************************************************************************
Copyright (c) 2008-2009, Motorola, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted
provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this list of conditions
  and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions
  and the following disclaimer in the documentation and/or other materials provided with the
  distribution.
- Neither the name of the Motorola, Inc. nor the names of its contributors may be used to endorse or
  promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "alarm.h"
#include "events.h"
#include "hardware.h"
#include "screen.h"

#include <sys/reboot.h>
#include <sys/time.h>
#define LOG_TAG "CHARGE_ONLY_MODE"
#include <utils/Log.h>

static int quit = 0;
static int launched = 0;
static int shutdown = 0;
static int powerup = 0;

void power_event(int update_leds);

#define ANIMATION_TIMEOUT (1000 / 2)
#define POWERUP_VOLTAGE 3600000

#define FS_PATH "/data/chargeonlymode"
/* The follow two funcs are used to handle kpanic and wdreset case in COM */
int save_mode(const char *path)
{
        char mode[]="charging";
        int fd = open(path, O_RDWR | O_CREAT, 660);
        if (fd < 0)
                return -1;
        write(fd, mode, strlen(mode)+1);
        close(fd);
        return 0;
}

int clear_mode(const char *path)
{
        int r = truncate(path, 0);
        sync();
        ALOGD("clear mode return %d\n", r);
	sleep(1);
        return r;
}

/* This drives the bubbling animation */
void animation_alarm(void *_)
{
	alarm_set_relative(animation_alarm, NULL, ANIMATION_TIMEOUT);
	power_event(0);
}

void screen_brightness_animation_alarm2(void *_)
{
	set_brightness(0.0);

	alarm_cancel(animation_alarm);

	if (shutdown)
	{
		clear_mode(FS_PATH);
		reboot(RB_POWER_OFF);
	}
        display_blank();
}

void screen_brightness_animation_alarm1(void *_)
{
	set_brightness(0.4);
	/* Bright for 10s */
	alarm_set_relative(screen_brightness_animation_alarm2, NULL, 10000);
}

void screen_brightness_animation_start(int animation_timeout)
{
        display_unblank();

	set_brightness(0.8);
	alarm_set_relative(screen_brightness_animation_alarm1, NULL, 5000);
	alarm_set_relative(animation_alarm, NULL, animation_timeout);
}

void power_key_alarm(void *_)
{
	/* Delete the timers before reboot the phone. */
	alarm_cancel(screen_brightness_animation_alarm2);
	alarm_cancel(screen_brightness_animation_alarm1);
	alarm_cancel(animation_alarm);

	/* Check to see if we should run the memory test first. */
	if(launched)
	{
		system("/system/bin/memtest_mode");
	}
	/* Set powerup reason outof-charge-only to MBM. */
	clear_mode(FS_PATH);
	int ret = __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "outofcharge");
	ALOGD("reboot to outofcharge!\n");
	if(ret < 0) {
		ALOGD("reboot fail!\n");
	}
	else {
		quit = 1;
	}
}

void power_event(int update_leds)
{
	struct device_state state;
	static int powerup_latch = 0;

	/* Initialize to appease Klocwork */
	state.is_plugged_into_ac = 0;
	state.is_plugged_into_usb = 0;
	state.is_battery_present = 0;
	state.is_charging = 0;
	state.is_discharging = 0;
	state.is_unknown = 0;
	state.charge_level = 0;
	state.voltage_level = 0;

	get_device_state(&state);

	if (state.charge_level > 100)
		state.charge_level = 100;
	else if (state.charge_level < 0)
		state.charge_level = 0;

	/* Prevent showing anything but 0% until Power Voltage is high enough */
	if (state.voltage_level >= POWERUP_VOLTAGE) {
		powerup = 1;
		ALOGD("voltage ok for PU %d\n", state.voltage_level);
		powerup_latch = 1;
        }else{
		powerup = 0;
		ALOGD("voltage is %d, less than powerup voltage %d\n",
			state.voltage_level, POWERUP_VOLTAGE);
		if (!powerup_latch)
			state.charge_level = 0;
	}
	screen_update(state.charge_level,
			(state.is_discharging && (state.is_plugged_into_ac || state.is_plugged_into_usb))
			|| !state.is_battery_present
			|| state.is_unknown);
	if (update_leds)
		set_battery_led(&state);

	/* When the charger is unplugged the device shall shutdown at once. */
	if (!shutdown && !state.is_plugged_into_ac && !state.is_plugged_into_usb) {
		shutdown = 1;
		alarm_cancel(screen_brightness_animation_alarm2);
		alarm_cancel(screen_brightness_animation_alarm1);
		screen_brightness_animation_alarm2(NULL);
	}
}

void update_screen_on_wakeup_key(void)
{
	alarm_cancel(screen_brightness_animation_alarm2);
	alarm_cancel(animation_alarm);
	if (alarm_cancel(screen_brightness_animation_alarm1))
		/* This handles shutdown properly */
		screen_brightness_animation_alarm2(NULL);
	else
		screen_brightness_animation_start(ANIMATION_TIMEOUT);
}

void update_screen_on_wakeup_key2(void)
{
	alarm_cancel(screen_brightness_animation_alarm2);
	alarm_cancel(screen_brightness_animation_alarm1);
	alarm_cancel(animation_alarm);
	screen_brightness_animation_start(ANIMATION_TIMEOUT);
}

// Determine whether or not to launch the Memory Test backdoor.
// Secret sequence: Vol Up, Vol Up, Vol Down, Vol Down,
//                  Vol Up, Vol Up, Vol Down, Vol Down, Hold Powerkey.
int launch_sequence(int r)
{
	int sequence[9] = {
	                   EVENT_VOLUMEUP_KEY_DOWN,   EVENT_VOLUMEUP_KEY_DOWN,
	                   EVENT_VOLUMEDOWN_KEY_DOWN, EVENT_VOLUMEDOWN_KEY_DOWN,
	                   EVENT_VOLUMEUP_KEY_DOWN,   EVENT_VOLUMEUP_KEY_DOWN,
	                   EVENT_VOLUMEDOWN_KEY_DOWN, EVENT_VOLUMEDOWN_KEY_DOWN,
	                   EVENT_POWER_KEY_DOWN
	                  };

	static int counter = 0;

	switch(r)
	{
		case EVENT_VOLUMEDOWN_KEY_DOWN:
		case EVENT_VOLUMEUP_KEY_DOWN:
		case EVENT_POWER_KEY_DOWN:
			if(sequence[counter] == r)
			{
				counter++;
				ALOGD("increment launch counter to %d\n",counter);
			}
			else
			{
				counter = 0;
			}
			if(counter == (int) sizeof(sequence)/sizeof(int))
			{
				//Activated!
				ALOGI("Launched.\n");
				return(1);
			}
			break;
		default:
			break;
	}
	return 0;
}

int main()
{
	save_mode(FS_PATH);
	struct device_state old_state;
	get_device_state(&old_state);

	if (screen_init() < 0)
		goto err1;
	ev_init();

	uev_get(10000);

	/* To clean FB to avoid old data impact the COM images display*/
        display_blank();
        display_unblank();

	/* Set initialize image, screen brightness */
	set_button_brightness(0);
	screen_brightness_animation_start(1500);

	while (!quit) {
		int r, delay = alarm_get_time_until_next();
		if (delay < 1) {
			alarm_process();
			continue;
		}

		r = ev_get(delay);

		if(launch_sequence(r))
		{
			launched = 1;
		}

		/* Press below keys will wake the display and repeat the
		   display cycle:
                   Power key, Volume up/down key, Camera key.
                   Long press Power key will reboot the device. */
                switch (r) {
                        /* Power key */
                case EVENT_POWER_KEY_DOWN:
			if (powerup)
                        	alarm_set_relative(power_key_alarm, NULL, 1000);
                        break;
                case EVENT_POWER_KEY_UP:
                        alarm_cancel(power_key_alarm);
                        update_screen_on_wakeup_key();
                        break;
                        /* Other keys */
                case EVENT_VOLUMEDOWN_KEY_DOWN:
                        update_screen_on_wakeup_key2();
                        break;
                case EVENT_VOLUMEUP_KEY_DOWN:
                        update_screen_on_wakeup_key2();
                        break;
                case EVENT_CAMERA_KEY_DOWN:
                        update_screen_on_wakeup_key2();
                        break;
                        /* Battery events */
                case EVENT_BATTERY:
			power_event(0);
                        break;
                        /* Others */
                case EVENT_QUIT:
                        quit = 1;
                        break;
                default:
                        break;
                }
	}

	ev_exit();
	screen_uninit();

	set_brightness(1.0);
	set_battery_led(&old_state);
	clear_mode(FS_PATH);
	return 0;

err1:
	clear_mode(FS_PATH);
	return -1;

}
