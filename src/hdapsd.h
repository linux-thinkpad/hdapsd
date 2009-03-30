#include <stdio.h>

#define PID_FILE                "/var/run/hdapsd.pid"
#define HDAPS_POSITION_FILE	"/sys/devices/platform/hdaps/position"
#define MOUSE_ACTIVITY_FILE     "/sys/devices/platform/hdaps/keyboard_activity"
#define KEYBD_ACTIVITY_FILE     "/sys/devices/platform/hdaps/mouse_activity"
#define SAMPLING_RATE_FILE      "/sys/devices/platform/hdaps/sampling_rate"
#define AMS_POSITION_FILE	"/sys/devices/ams/current"
#define BUF_LEN                 40

#define FREEZE_SECONDS          1    /* period to freeze disk */
#define REFREEZE_SECONDS        0.1  /* period after which to re-freeze disk */
#define FREEZE_EXTRA_SECONDS    4    /* additional timeout for kernel timer */
#define DEFAULT_SAMPLING_RATE   50   /* default sampling frequency */
#define SIGUSR1_SLEEP_SEC       8    /* how long to sleep upon SIGUSR1 */

/* Magic threshold tweak factors, determined experimentally to make a
 * threshold of 10-20 behave reasonably.
 */
#define VELOC_ADJUST            30.0
#define ACCEL_ADJUST            (VELOC_ADJUST * 60)
#define AVG_VELOC_ADJUST        3.0

/* History depth for velocity average, in seconds */
#define AVG_DEPTH_SEC           0.3

/* Parameters for adaptive threshold */
#define RECENT_PARK_SEC        3.0    /* How recent is "recently parked"? */
#define THRESH_ADAPT_SEC       1.0    /* How often to (potentially) change
                                       * the adaptive threshold?           */
#define THRESH_INCREASE_FACTOR 1.1    /* Increase factor when recently
                                       * parked but user is typing      */
#define THRESH_DECREASE_FACTOR 0.9985 /* Decrease factor when not recently
                                       * parked, per THRESH_ADAPT_SEC sec. */
#define NEAR_THRESH_FACTOR     0.8    /* Fraction of threshold considered
                                       * being near the threshold.       */

/* Threshold for *continued* parking, as fraction of normal threshold */
#define PARKED_THRESH_FACTOR   NEAR_THRESH_FACTOR /* >= NEAR_THRESH_FACTOR */

enum interfaces {
	INTERFACE_NONE,
	INTERFACE_HDAPS,
	INTERFACE_AMS,
};

enum kernel {
	PROTECT,
	UNLOAD_HEADS,
};

struct list {
	char name[BUF_LEN];
	char protect_file[FILENAME_MAX];
	struct list *next;
};

