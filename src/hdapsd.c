/*
 * hdapsd.c - Read from the HDAPS (Hard Drive Active Protection System)
 *            and protect the drive if motion over threshold...
 *
 *            Derived from pivot.c by Robert Love.
 *
 * Copyright (C) 2005-2010 Jon Escombe <lists@dresco.co.uk>
 *                         Robert Love <rml@novell.com>
 *                         Shem Multinymous <multinymous@gmail.com>
 *                         Elias Oltmanns <eo@nebensachen.de>
 *                         Evgeni Golov <sargentd@die-welt.net>
 *                         Brice Arnould <brice.arnould+hdapsd@gmail.com>
 *
 * "Why does that kid keep dropping his laptop?"
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"
#include "hdapsd.h"
#include "input-helper.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <getopt.h>
#include <linux/input.h>
#include <syslog.h>
#include <dirent.h>
#include <libconfig.h>

#ifndef config_error_file
#define config_error_file(x) cfg_file
#endif


static int verbose = 0;
static int pause_now = 0;
static int dry_run = 0;
static int poll_sysfs = 0;
static int hardware_logic = 0;
static int force_software_logic = 0;
static int sampling_rate = 0;
static int running = 1;
static int background = 0;
static int dosyslog = 0;
static int forcerotational = 0;
static int use_leds = 1;

char pid_file[FILENAME_MAX] = "";
int hdaps_input_fd = 0;
int hdaps_input_nr = -1;
int freefall_fd = -1;

struct list *disklist = NULL;
enum kernel kernel_interface = UNLOAD_HEADS;
enum interfaces position_interface = INTERFACE_NONE;

/*
 * printlog (stream, fmt) - print the formatted message to syslog
 *                          or to the defined stream
 */
void printlog (FILE *stream, const char *fmt, ...)
{
	time_t now;
	int len = sizeof(fmt);

	char msg[len+1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, len+1024, fmt, ap);
	va_end(ap);

	if (dosyslog)
	        syslog(LOG_INFO, "%s", msg);
	else {
		now = time((time_t *)NULL);
		fprintf(stream, "%.24s: %s\n", ctime(&now), msg);
	}
}

/*
 * slurp_file - read the content of a file (up to BUF_LEN-1) into a string.
 *
 * We open and close the file on every invocation, which is lame but due to
 * several features of sysfs files:
 *
 *	(a) Sysfs files are seekable.
 *	(b) Seeking to zero and then rereading does not seem to work.
 *
 * If I were king--and I will be one day--I would have made sysfs files
 * nonseekable and only able to return full-size reads.
 */
static int slurp_file (const char* filename, char* buf)
{
	int ret;
	int fd = open (filename, O_RDONLY);
	if (fd < 0) {
		printlog(stderr, "Could not open %s: %s.\nDo you have the hdaps module loaded?", filename, strerror(errno));
		return fd;
	}

	ret = read (fd, buf, BUF_LEN-1);
	if (ret < 0) {
		printlog(stderr, "Could not read from %s: %s", filename, strerror(errno));
	} else {
		buf[ret] = 0; /* null-terminate so we can parse safely */
	ret = 0;
	}

	if (close (fd))
		printlog(stderr, "Could not close %s: %s", filename, strerror(errno));

	return ret;
}

/*
 * read_position_from_hdaps() - read the (x,y) position pair from hdaps via sysfs files
 * This method is not recommended for frequent polling, since it causes unnecessary interrupts
 * and a phase difference between hdaps-to-EC polling vs. hdapsd-to-hdaps polling.
 */
static int read_position_from_hdaps (int *x, int *y)
{
	char buf[BUF_LEN];
	int ret;
	if ((ret = slurp_file(HDAPS_POSITION_FILE, buf)))
		return ret;
	return (sscanf (buf, "(%d,%d)\n", x, y) != 2);
}

/*
 * read_position_from_ams() - read the (x,y,z) position from AMS via sysfs file
 */
static int read_position_from_ams (int *x, int *y, int *z)
{
	char buf[BUF_LEN];
	int ret;
	if ((ret = slurp_file(AMS_POSITION_FILE, buf)))
		return ret;
	return (sscanf (buf, "%d %d %d\n", x, y, z) != 3);
}

/*
 * read_position_from_hp3d() - read the (x,y,z) position from HP3D via sysfs file
 */
static int read_position_from_hp3d (int *x, int *y, int *z)
{
	char buf[BUF_LEN];
	int ret;
	if ((ret = slurp_file(HP3D_POSITION_FILE, buf)))
		return ret;
	return (sscanf (buf, "(%d,%d,%d)\n", x, y, z) != 3);
}

/*
 * read_position_from_applesmc() - read the (x,y,z) position from APPLESMC
 * via sysfs file
 */
static int read_position_from_applesmc (int *x, int *y, int *z)
{
	char buf[BUF_LEN];
	int ret;
	if ((ret = slurp_file(APPLESMC_POSITION_FILE, buf)))
		return ret;
	return (sscanf (buf, "(%d,%d,%d)\n", x, y, z) != 3);
}

/*
 * read_position_from_sysfs() - read the position either from HDAPS or
 * from AMS or from HP3D
 * depending on the given interface.
 */
static int read_position_from_sysfs (int *x, int *y, int *z)
{
	if (position_interface == INTERFACE_HDAPS)
		return read_position_from_hdaps(x,y);
	else if (position_interface == INTERFACE_AMS)
		return read_position_from_ams(x,y,z);
	else if (position_interface == INTERFACE_HP3D)
		return read_position_from_hp3d(x,y,z);
	else if (position_interface == INTERFACE_APPLESMC)
		return read_position_from_applesmc(x,y,z);
	return -1;
}


/*
 * read_int() - read an integer from a file
 */
static int read_int (const char* filename)
{
	char buf[BUF_LEN];
	int ret;
	if ((ret = slurp_file(filename, buf)))
		return ret;
	if (sscanf (buf, "%d\n", &ret) != 1)
		return -EIO;
	return ret;
}

/*
 * write_int() - writes an integer to a file
 */
static int write_int (const char* filename, const int value)
{
	char buf[BUF_LEN];
	int fd;
	int size;
	fd = open (filename, O_WRONLY);
	if (fd < 0)
	        return -1;
	size = snprintf (buf, BUF_LEN, "%i\n", value);
	if (write (fd, buf, size) < 0)
	        return -1;
	return close (fd);
}

/*
 * get_km_activity() - returns 1 if there is keyboard or mouse activity
 */
static int get_km_activity ()
{
	if (position_interface != INTERFACE_HDAPS)
		return 0;
	if (read_int(MOUSE_ACTIVITY_FILE) == 1)
		return 1;
	if (read_int(KEYBD_ACTIVITY_FILE) == 1)
		return 1;
	return 0;
}

/*
 * read_position_from_inputdev() - read the (x,y,z) position pair and time from hdaps
 * via the hdaps input device. Blocks there is a change in position.
 * The x and y arguments should contain the last read values, since if one of them
 * doesn't change it will not be assigned.
 */
static int read_position_from_inputdev (int *x, int *y, int *z, double *utime)
{
	struct input_event ev;
	int len, done = 0;
	*utime = 0;
	while (1) {
		len = read(hdaps_input_fd, &ev, sizeof(struct input_event));
		if (len < 0) {
			printlog(stderr, "ERROR: failed reading from input device: /dev/input/event%d  (%s).", hdaps_input_nr, strerror(errno));
			return len;
		}
		if (len < (int)sizeof(struct input_event)) {
			printlog(stderr, "ERROR: short read from input device: /dev/input/event%d (%d bytes).", hdaps_input_nr, len);
			return -EIO;
		}
		switch (ev.type) {
			case EV_ABS: /* new X, Y or Z */
				switch (ev.code) {
					case ABS_X:
						*x = ev.value;
						break;
					case ABS_Y:
						*y = ev.value;
						break;
					case ABS_Z:
						*z = ev.value;
						break;
					default:
						continue;
				}
				break;
			case EV_SYN: /* X and Y now reflect latest measurement */
				done = 1;
				break;
			default:
				continue;
		}
		if (!*utime) /* first event's time is closest to reality */
			*utime = ev.time.tv_sec + ev.time.tv_usec/1000000.0;
		if (done)
			return 0;
	}
}


/*
 * write_protect() - park/unpark
 */
static int write_protect (const char *path, int val)
{
	int fd, ret;
	char buf[BUF_LEN];

	if (dry_run)
		return 0;

	snprintf(buf, sizeof(buf), "%d", val);

	fd = open (path, O_WRONLY);
	if (fd < 0) {
		printlog (stderr, "Could not open %s", path);
		return fd;
	}

	ret = write (fd, buf, strlen(buf));

	if (ret < 0) {
		printlog (stderr, "Could not write to %s.\nDoes your kernel/drive support IDLE_IMMEDIATE with UNLOAD?", path);
		goto out;
	}
	ret = 0;

out:
	if (close (fd))
		printlog (stderr, "Could not close %s", path);

	return ret;
}

double get_utime (void)
{
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);
	if (ret) {
		perror("gettimeofday");
		exit(1);
	}
	return tv.tv_sec + tv.tv_usec/1000000.0;
}

/*
 * SIGUSR1_handler - Handler for SIGUSR1, sleeps for a few seconds. Useful when suspending laptop.
 */
void SIGUSR1_handler (int sig)
{
	signal(SIGUSR1, SIGUSR1_handler);
	pause_now = 1;
}

/*
 * SIGTERM_handler - Handler for SIGTERM, deletes the pidfile and exits.
 */
void SIGTERM_handler (int sig)
{
	signal(SIGTERM, SIGTERM_handler);
	running = 0;
}

/*
 * version() - display version information and exit
 */
void version ()
{
	printf(PACKAGE_STRING"\n");
	exit(0);
}

/*
 * usage() - display usage instructions and exit
 */
void usage ()
{
	printf("Usage: "PACKAGE_NAME" [OPTIONS]\n");
	printf("\n");
	printf("   -c --cfgfile=<cfgfile>            Load configuration from <cfgfile>.\n");
	printf("   -d --device=<device>              <device> is likely to be hda or sda.\n");
	printf("                                     Can be given multiple times\n");
	printf("                                     to protect multiple devices.\n");
	printf("   -f --force                        Force unloading heads, even if kernel thinks\n");
	printf("                                     differently (on pre ATA7 drives).\n");
	printf("                                     This only works when adding devices by hand (-d).\n");
	printf("   -r --force-rotational             Autodetect drives as rotational, even if\n");
	printf("                                     kernel thinks they are not.\n");
	printf("   -s --sensitivity=<sensitivity>    How sensitive "PACKAGE_NAME" should be to movements.\n");
	printf("                                     Defaults to 15, higher value means less\n");
	printf("                                     sensitive.\n");
	printf("   -a --adaptive                     Adaptive threshold (automatic increase\n");
	printf("                                     when the built-in keyboard/mouse are used).\n");
	printf("   -v --verbose                      Get verbose statistics.\n");
	printf("   -b --background                   Run the process in the background.\n");
	printf("   -p --pidfile[=<pidfile>]          Create a pid file when running\n");
	printf("                                     in background.\n");
	printf("                                     If <pidfile> is not specified,\n");
	printf("                                     it's set to %s.\n", PID_FILE);
	printf("   -t --dry-run                      Don't actually park the drive.\n");
	printf("   -y --poll-sysfs                   Force use of sysfs interface to\n");
	printf("                                     accelerometer.\n");
	printf("   -H --hardware-logic               Use the hardware fall detection logic instead of\n");
	printf("                                     the software one (-s/--sensitivity and -a/--adaptive\n");
	printf("                                     have no effect in this mode).\n");
	printf("   -S --software-logic               Use the software fall detection logic even if the\n");
	printf("                                     hardware one is available.\n");
	printf("   -L --no-leds                      Don't blink the LEDs.\n");
	printf("   -l --syslog                       Log to syslog instead of stdout/stderr.\n");
	printf("\n");
	printf("   -V --version                      Display version information and exit.\n");
	printf("   -h --help                         Display this message and exit.\n");
	printf("\n");
	printf("You can send SIGUSR1 to deactivate "PACKAGE_NAME" for %d seconds.\n",
		SIGUSR1_SLEEP_SEC);
	printf("\n");
	printf("Send bugs, comments and suggestions to "PACKAGE_BUGREPORT"\n");
	exit(1);
}

/*
 * check_thresh() - compare a value to the threshold
 */
void check_thresh (double val_sqr, double thresh, int* above, int* near,
                   char* reason_out, char reason_mark)
{
	if (val_sqr > thresh*thresh*NEAR_THRESH_FACTOR*NEAR_THRESH_FACTOR) {
		*near = 1;
		*reason_out = tolower(reason_mark);
	}
	if (val_sqr > thresh*thresh) {
		*above = 1;
		*reason_out = toupper(reason_mark);
	}
}

/*
 * analyze() - make a decision on whether to park given present readouts
 *             (remembers some past data in local static variables).
 * Computes and checks 3 values:
 *   velocity:     current position - prev position / time delta
 *   acceleration: current velocity - prev velocity / time delta
 *   average velocity: exponentially decaying average of velocity,
 *                     weighed by time delta.
 * The velocity and acceleration tests respond quickly to short sharp shocks,
 * while the average velocity test catches long, smooth movements (and
 * averages out measurement noise).
 * The adaptive threshold, if enabled, increases when (built-in) keyboard or
 * mouse activity happens shortly after some value was above or near the
 * adaptive threshold. The adaptive threshold slowly decreases back to the
 * base threshold when no value approaches it.
 */
int analyze (int x, int y, double unow, double base_threshold,
             int adaptive, int parked)
{
	static int x_last = 0, y_last = 0;
	static double unow_last = 0, x_veloc_last = 0, y_veloc_last = 0;
	static double x_avg_veloc = 0, y_avg_veloc = 0;
	static int history = 0; /* how many recent valid samples? */
	static double adaptive_threshold = -1; /* current adaptive thresh */
	static int last_thresh_change = 0; /* last adaptive thresh change */
	static int last_near_thresh = 0; /* last time we were near thresh */
	static int last_km_activity; /* last time kbd/mouse activity seen */

	double udelta, x_delta, y_delta, x_veloc, y_veloc, x_accel, y_accel;
	double veloc_sqr, accel_sqr, avg_veloc_sqr;
	double exp_weight;
	double threshold; /* transient threshold for this iteration */
	char reason[4]; /* "which threshold reached?" string for verbose */
	int recently_near_thresh;
	int above = 0, near = 0; /* above threshold, near threshold */

	/* Adaptive threshold adjustment  */
	if (adaptive_threshold<0) /* first invocation */
		adaptive_threshold = base_threshold;
	recently_near_thresh = unow < last_near_thresh + RECENT_PARK_SEC;
 	if (adaptive && recently_near_thresh && get_km_activity())
		last_km_activity = unow;
	if (adaptive && unow > last_thresh_change + THRESH_ADAPT_SEC) {
		if (recently_near_thresh) {
			if (last_km_activity > last_near_thresh &&
			    last_km_activity > last_thresh_change) {
				/* Near threshold and k/m activity */
				adaptive_threshold *= THRESH_INCREASE_FACTOR;
				last_thresh_change = unow;
			}
		} else {
			/* Recently never near threshold */
			adaptive_threshold *= THRESH_DECREASE_FACTOR;
			if (adaptive_threshold < base_threshold)
				adaptive_threshold = base_threshold;
			last_thresh_change = unow;
		}
	}

	/* compute deltas */
	udelta = unow - unow_last;
	x_delta = x - x_last;
	y_delta = y - y_last;

	/* compute velocity */
	x_veloc = x_delta/udelta;
	y_veloc = y_delta/udelta;
	veloc_sqr = x_veloc*x_veloc + y_veloc*y_veloc;

	/* compute acceleration */
	x_accel = (x_veloc - x_veloc_last)/udelta;
	y_accel = (y_veloc - y_veloc_last)/udelta;
	accel_sqr = x_accel*x_accel + y_accel*y_accel;

	/* compute exponentially-decaying velocity average */
	exp_weight = udelta/AVG_DEPTH_SEC; /* weight of this sample */
	exp_weight = 1 - 1.0/(1+exp_weight); /* softly clamped to 1 */
	x_avg_veloc = exp_weight*x_veloc + (1-exp_weight)*x_avg_veloc;
	y_avg_veloc = exp_weight*y_veloc + (1-exp_weight)*y_avg_veloc;
	avg_veloc_sqr = x_avg_veloc*x_avg_veloc + y_avg_veloc*y_avg_veloc;

	threshold = adaptive_threshold;
	if (parked) /* when parked, be reluctant to unpark */
		threshold *= PARKED_THRESH_FACTOR;

	/* Threshold test (uses Pythagoras's theorem) */
	strncpy(reason, "   ", 3);

	check_thresh(veloc_sqr, threshold*VELOC_ADJUST,
	             &above, &near, reason+0, 'V');
	check_thresh(accel_sqr, threshold*ACCEL_ADJUST,
	             &above, &near, reason+1, 'A');
	check_thresh(avg_veloc_sqr, threshold*AVG_VELOC_ADJUST,
	             &above, &near, reason+2, 'X');

	if (verbose) {
		printf("dt=%5.3f  "
		       "dpos=(%3g,%3g)  "
		       "vel=(%6.1f,%6.1f)*%g  "
		       "acc=(%6.1f,%6.1f)*%g  "
		       "avg_vel=(%6.1f,%6.1f)*%g  "
		       "thr=%.1f  "
		       "%s\n",
		       udelta,
		       x_delta, y_delta,
		       x_veloc/VELOC_ADJUST,
		       y_veloc/VELOC_ADJUST,
		       VELOC_ADJUST*1.0,
		       x_accel/ACCEL_ADJUST,
		       y_accel/ACCEL_ADJUST,
		       ACCEL_ADJUST*1.0,
		       x_avg_veloc/AVG_VELOC_ADJUST,
		       y_avg_veloc/AVG_VELOC_ADJUST,
		       AVG_VELOC_ADJUST*1.0,
		       threshold,
		       reason);
	}

	if (udelta>1.0) { /* Too much time since last (resume from suspend?) */
		history = 0;
		x_avg_veloc = y_avg_veloc = 0;
	}

	if (history<2) { /* Not enough data for meaningful result */
		above = 0;
		near = 0;
		++history;
	}

	if (near)
		last_near_thresh = unow;

	x_last = x;
	y_last = y;
	x_veloc_last = x_veloc;
	y_veloc_last = y_veloc;
	unow_last = unow;

	return above;
}

/*
 * add_disk (disk) - add the given disk to the global disklist
 */
void add_disk (char* disk)
{
	char protect_file[FILENAME_MAX] = "";
	if (kernel_interface == UNLOAD_HEADS)
		snprintf(protect_file, sizeof(protect_file), UNLOAD_HEADS_FMT, disk);
	else {
		snprintf(protect_file, sizeof(protect_file), QUEUE_PROTECT_FMT, disk);
	}

	if (disklist == NULL) {
		disklist = (struct list *)malloc(sizeof(struct list));
		if (disklist == NULL) {
			printlog(stderr, "Error allocating memory.");
			exit(EXIT_FAILURE);
		}
		else {
			strncpy(disklist->name, disk, sizeof(disklist->name));
			strncpy(disklist->protect_file, protect_file, sizeof(disklist->protect_file));
			disklist->next = NULL;
		}
	}
	else {
		struct list *p = disklist;
		while (p->next != NULL)
			p = p->next;
		p->next = (struct list *)malloc(sizeof(struct list));
		if (p->next == NULL) {
			printlog(stderr, "Error allocating memory.");
			exit(EXIT_FAILURE);
		}
		else {
			strncpy(p->next->name, disk, sizeof(p->next->name));
			strncpy(p->next->protect_file, protect_file, sizeof(p->next->protect_file));
			p->next->next = NULL;
		}
	}
}

/*
 * free_disk (disk) - free the allocated memory
 */
void free_disk (struct list *disk)
{
	if (disk != NULL) {
		if (disk->next != NULL)
			free_disk(disk->next);
		free(disk);
	}
}

/*
 * select_interface() - search for an interface we can read our position from
 */
int select_interface (int modprobe)
{
	int fd;

	char *modules[] = {"hdaps_ec", "hdaps", "ams", "hp_accel", "applesmc", "smo8800"};
	int mod_index;
	char command[64];
	position_interface = INTERFACE_NONE;

	if (modprobe) {
		for (mod_index = 0; mod_index < sizeof(modules)/sizeof(modules[0]); mod_index++) {
			snprintf(command, sizeof(command), "modprobe %s 1>/dev/null 2>/dev/null", modules[mod_index]);
			system(command);
		}
	}

	/* We don't know yet which interface to use, try HDAPS */
	fd = open (HDAPS_POSITION_FILE, O_RDONLY);
	if (fd >= 0) { /* yes, we are hdaps */
		close(fd);
		position_interface = INTERFACE_HDAPS;
	}
	if (position_interface == INTERFACE_NONE) {
		/* We still don't know which interface to use, try AMS */
		fd = open(AMS_POSITION_FILE, O_RDONLY);
		if (fd >= 0) { /* yes, we are ams */
			close(fd);
			position_interface = INTERFACE_AMS;
		}
	}
	if (position_interface == INTERFACE_NONE && !force_software_logic) {
		/* We still don't know which interface to use, try FREEFALL */
		fd = open(FREEFALL_FILE, FREEFALL_FD_FLAGS);
		if (fd >= 0) { /* yes, we are freefall */
			close(fd);
			position_interface = INTERFACE_FREEFALL;
			hardware_logic = 1;
		}
	}
	if (position_interface == INTERFACE_NONE) {
		/* We still don't know which interface to use, try HP3D */
		fd = open(HP3D_POSITION_FILE, O_RDONLY);
		if (fd >= 0) { /* yes, we are hp3d */
			close(fd);
			position_interface = INTERFACE_HP3D;
		}
	}
	if (position_interface == INTERFACE_NONE) {
		/* We still don't know which interface to use, try APPLESMC */
		fd = open(APPLESMC_POSITION_FILE, O_RDONLY);
		if (fd >= 0) { /* yes, we are applesmc */
			close(fd);
			position_interface = INTERFACE_APPLESMC;
		}
	}
	return position_interface;
}

/*
 * autodetect_devices()
 */
int autodetect_devices ()
{
	int num_devices = 0;
	DIR *dp;
	struct dirent *ep;
	dp = opendir(SYSFS_BLOCK);
	if (dp != NULL) {
		while ((ep = readdir(dp))) {
			char path[FILENAME_MAX];
			char removable[FILENAME_MAX];
			char rotational[FILENAME_MAX];
			snprintf(removable, sizeof(removable), REMOVABLE_FMT, ep->d_name);
			snprintf(rotational, sizeof(rotational), ROTATIONAL_FMT, ep->d_name);

			if (kernel_interface == UNLOAD_HEADS)
				snprintf(path, sizeof(path), UNLOAD_HEADS_FMT, ep->d_name);
			else
				snprintf(path, sizeof(path), QUEUE_PROTECT_FMT, ep->d_name);

			if (access(path, F_OK) == 0 && read_int(removable) == 0 && read_int(path) >= 0) {
				if (read_int(rotational) == 1 || forcerotational) {
					printlog(stdout, "Adding autodetected device: %s", ep->d_name);
					add_disk(ep->d_name);
					num_devices++;
				}
				else {
					printlog(stdout, "Not adding autodetected device \"%s\", it seems not to be a rotational drive.", ep->d_name);
				}
			}
		}
		(void)closedir(dp);
	}
	return num_devices;
}

/*
 * main() - loop forever, reading the hdaps values and
 *          parking/unparking as necessary
 */
int main (int argc, char** argv)
{
	struct utsname sysinfo;
	struct list *p = NULL;
	int c, park_now, protect_factor;
	int x = 0, y = 0, z = 0;
	int fd, i, ret, threshold = 15, adaptive = 0,
	pidfile = 0, parked = 0, forceadd = 0;
	double unow = 0, parked_utime = 0;
	config_t cfg;
	config_setting_t *setting;
	char cfg_file[FILENAME_MAX] = CONFIG_FILE;
	int cfgfile = 0;
	const char *tmpcstr;

	struct option longopts[] =
	{
		{"device", required_argument, NULL, 'd'},
		{"sensitivity", required_argument, NULL, 's'},
		{"adaptive", no_argument, NULL, 'a'},
		{"verbose", no_argument, NULL, 'v'},
		{"background", no_argument, NULL, 'b'},
		{"cfgfile", required_argument, NULL, 'c'},
		{"pidfile", optional_argument, NULL, 'p'},
		{"dry-run", no_argument, NULL, 't'},
		{"poll-sysfs", no_argument, NULL, 'y'},
		{"hardware-logic", no_argument, NULL, 'H'},
		{"software-logic", no_argument, NULL, 'S'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"no-leds", no_argument, NULL, 'L'},
		{"syslog", no_argument, NULL, 'l'},
		{"force", no_argument, NULL, 'f'},
		{"force-rotational", no_argument, NULL, 'r'},
		{NULL, 0, NULL, 0}
	};

	if (uname(&sysinfo) < 0 || strcmp("2.6.27", sysinfo.release) <= 0) {
		protect_factor = 1000;
		kernel_interface = UNLOAD_HEADS;
	}
	else {
		protect_factor = 1;
		kernel_interface = PROTECT;
	}

	openlog(PACKAGE_NAME, LOG_PID, LOG_DAEMON);

	while ((c = getopt_long(argc, argv, "d:s:vbac:p::tyHSVhLlfr", longopts, NULL)) != -1) {
		switch (c) {
			case 'd':
				add_disk(optarg);
				break;
			case 's':
				threshold = atoi(optarg);
				break;
			case 'b':
				background = 1;
				break;
			case 'a':
				adaptive = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'c':
				cfgfile = 1;
				snprintf(cfg_file, sizeof(cfg_file), "%s", optarg);
				break;
			case 'p':
				pidfile = 1;
				if (optarg == NULL) {
					snprintf(pid_file, sizeof(pid_file), "%s", PID_FILE);
				} else {
					snprintf(pid_file, sizeof(pid_file), "%s", optarg);
				}
				break;
			case 't':
				printlog(stdout, "Dry run, will not actually park heads or freeze queue.");
				dry_run = 1;
				break;
			case 'y':
				poll_sysfs = 1;
				break;
			case 'H':
				hardware_logic = 1;
				position_interface = INTERFACE_FREEFALL;
				break;
			case 'S':
				force_software_logic = 1;
				break;
			case 'V':
				version();
				break;
			case 'l':
				dosyslog = 1;
				break;
			case 'L':
				use_leds = 0;
				break;
			case 'f':
				forceadd = 1;
				break;
			case 'r':
				forcerotational = 1;
				break;
			case 'h':
			default:
				usage();
				break;
		}
	}

	printlog(stdout, "Starting "PACKAGE_NAME);

	config_init(&cfg);
	if (access(cfg_file, F_OK) == 0) {
		if (!config_read_file(&cfg, cfg_file)) {
			printlog(stderr, "%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
			config_destroy(&cfg);
			free_disk(disklist);
			return 1;
		}

		if (disklist == NULL) {
			setting = config_lookup(&cfg, "device");
			if (setting != NULL) {
				if (config_setting_is_array(setting)) {
					for (i = 0; i<config_setting_length(setting); i++) {
						add_disk((char *) config_setting_get_string_elem(setting, i));
					}
				} else if (config_setting_is_scalar(setting)) {
					add_disk((char *) config_setting_get_string(setting));
				}
			}
		}

		if (threshold == 15) {
			config_lookup_int(&cfg, "sensitivity", &threshold);
		}

		if (adaptive == 0) {
			config_lookup_bool(&cfg, "adaptive", &adaptive);
		}

		if (background == 0) {
			config_lookup_bool(&cfg, "background", &background);
		}

		if (pidfile == 0) {
			if (config_lookup_string(&cfg, "pidfile", &tmpcstr)) {
				pidfile = 1;
				strncpy(pid_file, tmpcstr, strlen(tmpcstr));
			}
		}

		if (dosyslog == 0) {
			config_lookup_bool(&cfg, "syslog", &dosyslog);
		}
	} else if (cfgfile) {
		printlog(stderr, "Could not open configuration file %s.", cfg_file);
		config_destroy(&cfg);
		free_disk(disklist);
		return 1;
	}

	if (disklist && forceadd) {
		char protect_method[FILENAME_MAX] = "";
		p = disklist;
		while (p != NULL) {
			snprintf(protect_method, sizeof(protect_method), QUEUE_METHOD_FMT, p->name);
			if (kernel_interface == UNLOAD_HEADS)
				fd = open (p->protect_file, O_RDWR);
			else
				fd = open (protect_method, O_RDWR);
			if (fd > 0) {
				if (kernel_interface == UNLOAD_HEADS)
					ret = write(fd, FORCE_UNLOAD_HEADS, strlen(FORCE_UNLOAD_HEADS));
				else
					ret = write(fd, FORCE_PROTECT_METHOD, strlen(FORCE_PROTECT_METHOD));
				if (ret == -1)
					printlog(stderr, "Could not forcely enable UNLOAD feature for %s", p->name);
				else
					printlog(stdout, "Forcely enabled UNLOAD for %s", p->name);
				close(fd);
			}
			else
				printlog(stderr, "Could not open %s for forcely enabling UNLOAD feature", p->protect_file);

			p = p->next;
		}
	}

	if (disklist == NULL) {
		printlog(stdout, "WARNING: You did not supply any devices to protect, trying autodetection.");
		if (autodetect_devices() < 1)
			printlog(stderr, "Could not detect any devices.");
	}

	if (disklist == NULL)
		usage(argv);

	/* Let's see if we're on a ThinkPad or on an *Book */
	if (!position_interface)
		select_interface(0);
	if (!position_interface)
		select_interface(1);

	if (!position_interface && !hardware_logic) {
		printlog(stdout, "Could not find a suitable interface");
		return -1;
	}
	else
		printlog(stdout, "Selected interface: %s", interface_names[position_interface]);
	if (hardware_logic) {
		/* Open the file representing the hardware decision */
	        freefall_fd = open (FREEFALL_FILE, FREEFALL_FD_FLAGS);
		if (freefall_fd < 0) {
				printlog(stdout,
				        "ERROR: Failed openning the hardware logic file (%s). "
					"It is probably not supported on your system.",
				        strerror(errno));
				return errno;
		}
		else {
			printlog (stdout, "Uses hardware logic from " FREEFALL_FILE);
		}
	}
	if (!poll_sysfs && !hardware_logic) {
		if (position_interface == INTERFACE_HDAPS) {
			hdaps_input_nr = device_find_byphys("hdaps/input1");
			hdaps_input_fd = device_open(hdaps_input_nr);
			if (hdaps_input_fd < 0) {
				printlog(stdout,
				        "WARNING: Could not find hdaps input device (%s). "
				        "You may be using an incompatible version of the hdaps module. "
				        "Falling back to reading the position from sysfs (uses more power).\n"
				        "Use '-y' to silence this warning.",
				        strerror(errno));
				poll_sysfs = 1;
			}
			else {
				printlog(stdout, "Selected HDAPS input device: /dev/input/event%d", hdaps_input_nr);
			}
		} else if (position_interface == INTERFACE_AMS) {
			hdaps_input_nr = device_find_byname("Apple Motion Sensor");
			hdaps_input_fd = device_open(hdaps_input_nr);
			if (hdaps_input_fd < 0) {
				printlog(stdout,
					"WARNING: Could not find AMS input device, do you need to set joystick=1?");
				poll_sysfs = 1;
			}
			else {
				printlog(stdout, "Selected AMS input device /dev/input/event%d", hdaps_input_nr);
			}
		} else if (position_interface == INTERFACE_HP3D) {
			hdaps_input_nr = device_find_byname("ST LIS3LV02DL Accelerometer");
			hdaps_input_fd = device_open(hdaps_input_nr);
			if (hdaps_input_fd < 0) {
				printlog(stdout,
					"WARNING: Could not find HP3D input device");
				poll_sysfs = 1;
			}
			else {
				printlog(stdout, "Selected HP3D input device /dev/input/event%d", hdaps_input_nr);
			}
		} else if (position_interface == INTERFACE_APPLESMC) {
			hdaps_input_nr = device_find_byname("applesmc");
			hdaps_input_fd = device_open(hdaps_input_nr);
			if (hdaps_input_fd < 0) {
				printlog(stdout,
					"WARNING: Could not find APPLESMC input device");
				poll_sysfs = 1;
			}
			else {
				printlog(stdout, "Selected APPLESMC input device /dev/input/event%d", hdaps_input_nr);
			}
		}
	}
	if (position_interface != INTERFACE_HP3D && position_interface != INTERFACE_FREEFALL) {
		/* LEDs are not supported yet on other systems */
		use_leds = 0;
	}
	if (use_leds) {
		fd = open(HP3D_LED_FILE, O_WRONLY);
		if (fd < 0)
			use_leds = 0;
		else
			close(fd);
	}

	if (background) {
		verbose = 0;
		if (pidfile) {
			fd = open (pid_file, O_WRONLY | O_CREAT, 0644);
			if (fd < 0) {
				printlog (stderr, "Could not create pidfile: %s", pid_file);
				return 1;
			}
		}
		daemon(0,0);
		if (pidfile) {
			char buf[BUF_LEN];
			snprintf (buf, sizeof(buf), "%d\n", getpid());
			ret = write (fd, buf, strlen(buf));
			if (ret < 0) {
				printlog (stderr, "Could not write to pidfile %s", pid_file);
				return 1;
			}
			if (close (fd)) {
				printlog (stderr, "Could not close pidfile %s", pid_file);
				return 1;
			}
		}
	}

	mlockall(MCL_FUTURE);

	if (verbose) {
		p = disklist;
		while (p != NULL) {
			printf("disk: %s\n", p->name);
			p = p->next;
		}
		printf("threshold: %i\n", threshold);
		printf("read_method: %s\n", poll_sysfs ? "poll-sysfs" : (hardware_logic ? "hardware-logic" : "input-dev"));
	}

	/* check the protect attribute exists */
	/* wait for it if it's not there (in case the attribute hasn't been created yet) */
	p = disklist;
	while (p != NULL && !dry_run) {
		fd = open (p->protect_file, O_RDWR);
		if (background)
			for (i = 0; fd < 0 && i < 100; ++i) {
				usleep (100000);	/* 10 Hz */
				fd = open (p->protect_file, O_RDWR);
			}
		if (fd < 0) {
			printlog (stderr, "Could not open %s\nDoes your kernel/drive support IDLE_IMMEDIATE with UNLOAD?", p->protect_file);
			free_disk(disklist);
			config_destroy(&cfg);
			return 1;
		}
		close (fd);
		p = p->next;
	}

	/* see if we can read the sensor */
	/* wait for it if it's not there (in case the attribute hasn't been created yet) */
	if (!hardware_logic) {
		ret = read_position_from_sysfs (&x, &y, &z);
		if (background)
			for (i = 0; ret && i < 100; ++i) {
				usleep (100000);	/* 10 Hz */
				ret = read_position_from_sysfs (&x, &y, &z);
			}
		if (ret) {
			printlog(stderr, "Could not read position from sysfs.");
			return 1;
		}
	}

	/* adapt to the driver's sampling rate */
	if (position_interface == INTERFACE_HDAPS && access(HDAPS_SAMPLING_RATE_FILE, F_OK) == 0)
		sampling_rate = read_int(HDAPS_SAMPLING_RATE_FILE);
	else if (position_interface == INTERFACE_HP3D)
		sampling_rate = read_int(HP3D_SAMPLING_RATE_FILE);
	if (sampling_rate <= 0)
		sampling_rate = DEFAULT_SAMPLING_RATE;
	if (verbose)
		printf("sampling_rate: %d\n", sampling_rate);

	signal(SIGUSR1, SIGUSR1_handler);

	signal(SIGTERM, SIGTERM_handler);

	while (running) {
		if (!hardware_logic) { /* The decision is made by the software */
			/* Get statistics and decide what to do */
			if (poll_sysfs) {
				usleep (1000000/sampling_rate);
				ret = read_position_from_sysfs (&x, &y, &z);
				unow = get_utime(); /* microsec */
			} else {
				double oldunow = unow;
				int oldx = x, oldy = y, oldz = z;
				ret = read_position_from_inputdev (&x, &y, &z, &unow);

				/*
				 * The input device issues events only when the position changed.
				 * The analysis state needs to know how long the position remained
				 * unchanged, so send analyze() a fake retroactive update before sending
				 * the new one.
				 */
				if (!ret && oldunow && unow-oldunow > 1.5/sampling_rate)
					analyze(oldx, oldy, unow-1.0/sampling_rate, threshold, adaptive, parked);

			}

			if (ret) {
				if (verbose)
					printf("readout error (%d)\n", ret);
				continue;
			}

			park_now = analyze(x, y, unow, threshold, adaptive, parked);
		}
		else /* if (hardware_logic) */ {
			unsigned char count; /* Number of fall events */
			if (!parked) {
				/* Wait for the hardware to notify a fall */
				ret = read(freefall_fd, &count, sizeof(count));
			}
			else {
				/*
				 * Poll to check if we no longer are falling
				 * (hardware_logic polls only when parked)
				 */
				usleep (1000000/sampling_rate);
				fcntl (freefall_fd, F_SETFL, FREEFALL_FD_FLAGS|O_NONBLOCK);
				ret = read(freefall_fd, &count, sizeof(count));
				fcntl (freefall_fd, F_SETFL, FREEFALL_FD_FLAGS);
				/*
				 * If the error is EAGAIN then it is not a real error but
				 * a sign that the fall has ended
				 */
				if (ret != sizeof(count) && errno == EAGAIN) {
					count = 0; /* set fall events count to 0 */
					ret = sizeof(count); /* Validate count */
				}
			}
			/* handle read errors */
			if (ret != sizeof(count)) {
				if (verbose)
					printf("readout error (%d)\n", ret);
				continue;
			}
			/* Display the read values in verbose mode */
			if (verbose)
				printf ("HW=%u\n", (unsigned) count);
			unow = get_utime(); /* microsec */
			park_now = (count > 0);
		}

		if (park_now && !pause_now) {
			if (!parked || unow>parked_utime+REFREEZE_SECONDS) {
				/* Not frozen or freeze about to expire */
				p = disklist;
				while (p != NULL) {
					write_protect(p->protect_file,
					      (FREEZE_SECONDS+FREEZE_EXTRA_SECONDS) * protect_factor);
					p = p->next;
				}
				/*
				 * Write protect before any output (xterm, or
				 * whatever else is handling our stdout, may be
				 * swapped out).
				 */
				if (!parked) {
				        printlog(stdout, "parking");
					if (use_leds)
						write_int (HP3D_LED_FILE, 1);
				}
				parked = 1;
				parked_utime = unow;
			}
		} else {
			if (parked &&
			    (pause_now || unow>parked_utime+FREEZE_SECONDS)) {
				/* Sanity check */
				p = disklist;
				while (p != NULL) {
					if (!dry_run && !read_int(p->protect_file))
						printlog(stderr, "Error! Not parked when we "
						       "thought we were... (paged out "
					               "and timer expired?)");
					/* Freeze has expired */
					write_protect(p->protect_file, 0); /* unprotect */
					if (use_leds)
						write_int (HP3D_LED_FILE, 0);
					p = p->next;
				}
				parked = 0;
				printlog(stdout, "un-parking");
			}
			while (pause_now) {
				pause_now = 0;
				printlog(stdout, "pausing for %d seconds", SIGUSR1_SLEEP_SEC);
				sleep(SIGUSR1_SLEEP_SEC);
			}
		}

	}

	free_disk(disklist);
	config_destroy(&cfg);
	printlog(stdout, "Terminating "PACKAGE_NAME);
	closelog();
	if (pidfile)
		unlink(pid_file);
	munlockall();
	return ret;
}
