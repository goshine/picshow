#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "led_conf.h"
#include "log.h"

#define DEBUG	(ALL_DEBUG_SWITCH && 1)

int get_led_size(const char *confpath, int *x, int *y)
{
    Log(LOG_DEBUG, "function:%s --- paralist{confpath=%s}\n", __func__, confpath);

	FILE *fp = NULL;
	char buf[BUF_SIZE] = { '\0' };
	char *pos = NULL;

	if (NULL == (fp = fopen(confpath, "r"))) {
		Log(LOG_WARN, "function:%s --- %s\n", __func__, strerror(errno));
		return -1;
	}

	while (NULL != fgets(buf, sizeof(buf), fp)) {
		if (NULL != (pos = strstr(buf, CONF_KEY_LED_SIZE"="))) {
			if (2 == sscanf(pos + strlen(CONF_KEY_LED_SIZE"="), "%d*%d", x, y)) {
                Log(LOG_DEBUG, "function:%s --- x=%d,y=%d\n", __func__, *x, *y);
				return 0;
			} else
				return -1;
		}
	}

	return -1;
}
