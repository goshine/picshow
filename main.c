#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <signal.h>

#include "fbv.h"
#include "image_proc.h"
#include "log.h"

#define DEBUG			(ALL_DEBUG_SWITCH && 1)
#define PAN_STEPPING	20

static int opt_resol_x = RESOL_DEF;
static int opt_resol_y = RESOL_DEF;
static int opt_scale = SCALE_DEF;
static int opt_pos = POS_DEF;
static int opt_delay = DELAY_DEF;
static int opt_effect = EFF_DEF;

int isGif = 0;

void sighandler(int s)
{
	Log(LOG_ERROR, "function:%s --- paralist{s=%d}\n", __func__, s);

	_exit(128 + s);
}

int command_parse(int argc, char **argv)
{
    int i = 0;
    Log(LOG_DEBUG, "function:%s --- paralist{argc=%d}\n", __func__, argc);

    while (i < argc) {
        Log(LOG_DEBUG, "function:%s --- paralist{argv[%d]=%s}\n", __func__, i, argv[i]);
        i++;
	}

	struct option long_options[] = { { "resolution", required_argument, 0, 'r' }, // argument should be like 1024x768
	        { "scale", required_argument, 0, 's' }, // argument should be an integer from 50-200
	        { "position", required_argument, 0, 'p' }, // argument should be 0-POS_TOP 1-POS_CENTER 2-POS_BOTTOM
	        { "delay", required_argument, 0, 'd' }, // argument should be an integer >= 0
	        { "effect", required_argument, 0, 'e' }, // argument should be 0-EFF_DEF 1-EFF_WIN 2-EFF_PUSH 3-EFF_SPRED
	        { 0, 0, 0, 0 } };

	if (argc < 2) {
		Log(LOG_ERROR, "function:%s --- Usage : %s [option] [parameter] <filename>\n", __func__, argv[0]);
		exit(EXIT_FAILURE);
	}

	int opt;
	while (EOF != (opt = getopt_long_only(argc, argv, "r:s:p:d:e:", long_options, NULL))) {
		switch (opt) {
			case 'r': {
				if (NULL != optarg)
					opt_resol_x = atoi(optarg);

				char *p = NULL;
				if (NULL != (p = strstr(optarg, "x")))
					opt_resol_y = atoi(p + 1);
				break;
			}
			case 's': {
				if (NULL != optarg)
					opt_scale = atoi(optarg);
				break;
			}
			case 'p': {
				if (NULL != optarg)
					opt_pos = atoi(optarg);
				break;
			}
			case 'd': {
				if (NULL != optarg)
					opt_delay = atoi(optarg);
				break;
			}
			case 'e': {
				if (NULL != optarg)
					opt_effect = atoi(optarg);
				break;
			}
		}
	}

	return 0;
}

struct image *load_image(char *filename)
{
	int (*load)(char *, unsigned char *, unsigned char **, int, int);

	int ix_size, iy_size;

	if (fh_gif_id(filename)) {
		isGif = 1;
		return NULL;
	} else if (fh_png_id(filename)) {
		if (fh_png_getsize(filename, &ix_size, &iy_size) == FH_ERROR_OK) {
			load = fh_png_load;
			goto identified;
		}
	} else if (fh_jpeg_id(filename)) {
		if (fh_jpeg_getsize(filename, &ix_size, &iy_size) == FH_ERROR_OK) {
			load = fh_jpeg_load;
			goto identified;
		}
	} else if (fh_bmp_id(filename)) {
		if (fh_bmp_getsize(filename, &ix_size, &iy_size) == FH_ERROR_OK) {
			load = fh_bmp_load;
			goto identified;
		}
	}

    Log(LOG_DEBUG, "function:%s --- Image information:\n", __func__);
    Log(LOG_DEBUG, "        filename   : %s\n", filename);
    Log(LOG_DEBUG, "        resolution : %d x %d\n", ix_size, iy_size);

	return NULL;

	identified: {
		unsigned char * image = NULL;
		unsigned char * alpha = NULL;

		if (!(image = (unsigned char*) malloc(ix_size * iy_size * 3))) {
			Log(LOG_WARN, "function:%s --- image buffer malloc failed!\n", __func__);
			return NULL;
		}

		if (load(filename, image, &alpha, ix_size, iy_size) != FH_ERROR_OK) {
			Log(LOG_WARN, "function:%s --- image load failed!\n", __func__);
			free(image);
			return NULL;
		}

		struct image *i = NULL;
		if (!(i = (struct image *) malloc(sizeof(struct image)))) {
			Log(LOG_WARN, "function:%s --- image structure malloc failed!\n", __func__);
			free(image);
			free(alpha);
			return NULL;
		}

		memset(i, 0, sizeof(struct image));
		i->do_free = 0;
		i->width = ix_size;
		i->height = iy_size;
		i->rgb = image; // should be free after use
		i->alpha = alpha; // should be free after use
		i->do_free = 0;

		return i;
	}
}

int show_image(struct image *img, int delay)
{
	int x_offs = 0, y_offs = 0; // for position

	// 只要设定了分辨率参数，都按照分辨率处理图像，无论是否设定缩放
	if (RESOL_DEF != opt_resol_x && RESOL_DEF != opt_resol_y)
		do_resol_adjust(opt_resol_x, opt_resol_y, img);
	else
		do_scale_adjust(opt_scale, img);

	do_position_adjust(opt_pos, img, &x_offs, &y_offs);
	//do_display_effect(opt_effect);

	fb_display(img->rgb, img->alpha, img->width, img->height, 0, 0, x_offs, y_offs);
	do_display_delay(delay);

	return 0;
}

int main(int argc, char **argv)
{
	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGSEGV, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGABRT, sighandler);

	// step 1 : parse command
	command_parse(argc, argv);

	Log(LOG_INFO, "function:%s --- parameter:\n", __func__);
	Log(LOG_INFO, "          image name : %s\n", argv[optind]);
	Log(LOG_INFO, "          resolution : %d x %d\n", opt_resol_x, opt_resol_y);
	Log(LOG_INFO, "          scale      : %d\n", opt_scale);
	Log(LOG_INFO, "          position   : %d\n", opt_pos);
	Log(LOG_INFO, "          delay time : %d\n", opt_delay);

	// step 2 : load image file
	struct image *img = NULL; // should be free
	if (NULL == argv[optind] || (NULL == (img = load_image(argv[optind])) && 0 == isGif)) {
		Log(LOG_ERROR,
		        "function:%s --- %s Unable to access file or file format unknown!\n",
		        __func__, argv[optind]);
		exit(EXIT_FAILURE);
	}

	// step3 : show image
	if (isGif) {
		unsigned char * alpha = NULL;
		if (fh_gif_load(argv[optind], NULL, &alpha, 0, 0) != FH_ERROR_OK) {
			Log(LOG_WARN, "function:%s --- %s Image data is corrupt!\n", __func__, argv[optind]);
		}
		isGif = 0;
	} else
		show_image(img, opt_delay);

	if (img) {
		free(img->rgb);
		free(img->alpha);
		free(img);
	}

	exit(EXIT_SUCCESS);
}
