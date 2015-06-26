#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "image_proc.h"
#include "fbv.h"
#include "led_conf.h"
#include "log.h"

#define DEBUG	(ALL_DEBUG_SWITCH && 1)

int do_resol_adjust(int resol_x, int resol_y, struct image *img)
{
    Log(LOG_DEBUG, "function:%s --- paralist{resol_x=%d,resol_y=%d}\n", __func__, resol_x, resol_y);

	int screen_width = 0, screen_height = 0;
	unsigned char * new_image = NULL, *new_alpha = NULL;

//	get_led_size(CONF_PATH, &screen_width, &screen_height);

	if (RESOL_SCREEN == resol_x && RESOL_SCREEN == resol_y) {
		// fit to screen resolution
		resol_x = screen_width;
		resol_y = screen_height;
		new_image = color_average_resize(img->rgb, img->width, img->height, resol_x, resol_y);
	} else if ((0 < resol_x && 0 < resol_y)) {
		// specific resolution
		new_image = color_average_resize(img->rgb, img->width, img->height, resol_x, resol_y);
	} else
		// original resolution
		return -1;

	if (img->alpha)
		new_alpha = alpha_resize(img->alpha, img->width, img->height, resol_x, resol_y);

	free(img->rgb);
	free(img->alpha);

	img->rgb = new_image;
	img->alpha = new_alpha;
	img->width = resol_x;
	img->height = resol_y;

	return 0;
}

int do_scale_adjust(int scale, struct image *img)
{
    Log(LOG_DEBUG, "function:%s --- paralist{scale=%d}\n", __func__, scale);

	if (SCALE_DEF != scale && scale >= SCALE_MIN && scale <= SCALE_MAX) {
		int nx = 0, ny = 0;
		unsigned char * new_image = NULL, *new_alpha = NULL;

		nx = (int) img->width * scale / 100;
		ny = (int) img->height * scale / 100;

		new_image = color_average_resize(img->rgb, img->width, img->height, nx, ny);

		if (img->alpha)
			new_alpha = alpha_resize(img->alpha, img->width, img->height, nx, ny);

		free(img->rgb);
		free(img->alpha);

		img->rgb = new_image;
		img->alpha = new_alpha;
		img->width = nx;
		img->height = ny;
	}

	return 0;
}

int do_position_adjust(int pos, struct image *img, int *xoff, int *yoff)
{
	int screen_width = 0, screen_height = 0;

//	get_led_size(CONF_PATH, &screen_width, &screen_height);

	switch (pos) {
		case POS_TOP:
			*xoff = 0;
			*yoff = 0;
			break;
		case POS_CENTER:
			*xoff = (screen_width - img->width) / 2;
			*yoff = (screen_height - img->height) / 2;
			break;
		case POS_BOTTOM:
			*xoff = 0;
			*yoff = screen_height - img->height;
			break;
		default:
			*xoff = 0;
			*yoff = 0;
			break;
	}

    Log(LOG_DEBUG, "function:%s --- pos=%d, xoff=%d, yoff=%d\n", __func__, pos, *xoff, *yoff);

	return 0;
}

int do_display_effect(int effect)
{
	return 0;
}

int do_display_delay(int msec)
{
    Log(LOG_DEBUG, "function:%s --- {paralist:msec=%d}\n", __func__, msec);

	struct timeval tv;
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	if (DELAY_DEF != msec) {
		tv.tv_sec = 0;
		tv.tv_usec = msec * 1000;
		return select(1, &fds, NULL, NULL, &tv);
	} else
		return select(1, &fds, NULL, NULL, NULL);

}
