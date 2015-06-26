#ifndef __IMAGE_PROC_H__
#define __IMAGE_PROC_H__

#include "fbv.h"

#define POS_DEF			POS_TOP
#define POS_TOP			0
#define POS_CENTER		1
#define POS_BOTTOM		2

#define EFF_DEF		0
#define EFF_WIN		1
#define EFF_PUSH	2
#define EFF_SPRED	3

#define DELAY_DEF	-1

#define RESOL_DEF		-1
#define RESOL_SCREEN	0

#define SCALE_DEF	100
#define SCALE_MAX	200
#define SCALE_MIN	50

int do_resol_adjust(int resol_x, int resol_y, struct image *img);
int do_scale_adjust(int scale, struct image *img);
int do_position_adjust(int pos, struct image *img, int *xoff, int *yoff);
int do_display_effect(int effect);
int do_display_delay(int sec);

#endif
