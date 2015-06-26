/*`
 fbv  --  simple image viewer for the linux framebuffer
 Copyright (C) 2000  Tomasz Sterna
 Copyright (C) 2003  Mateusz Golicz

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <string.h>
#include <errno.h>
#include <linux/fb.h>

#include "fbv.h"
#include "log.h"

#define DEBUG	(ALL_DEBUG_SWITCH && 1)

unsigned short red[256], green[256], blue[256];
struct fb_cmap map332 = { 0, 256, red, green, blue, NULL };
unsigned short red_b[256], green_b[256], blue_b[256];
struct fb_cmap map_back = { 0, 256, red_b, green_b, blue_b, NULL };

int openFB(const char *name);
void closeFB(int fh);
void getVarScreenInfo(int fh, struct fb_var_screeninfo *var);
void setVarScreenInfo(int fh, struct fb_var_screeninfo *var);
void getFixScreenInfo(int fh, struct fb_fix_screeninfo *fix);
void set332map(int fh);
void* convertRGB2FB(int fh, unsigned char *rgbbuff, unsigned long count, int bpp, int *cpp);
void blit2FB(int fh, void *fbbuff, unsigned char *alpha, unsigned int pic_xs, unsigned int pic_ys, unsigned int scr_xs,
        unsigned int scr_ys, unsigned int xp, unsigned int yp, unsigned int xoffs, unsigned int yoffs, int cpp);

void fb_display(unsigned char *rgbbuff, unsigned char * alpha, int x_size, int y_size, int x_pan, int y_pan, int x_offs,
        int y_offs)
{
    Log(LOG_DEBUG, "function:%s --- paralist{x_size=%d,y_size=%d,x_pan=%d,y_pan=%d,x_offs=%d,y_offs=%d}\n",
		        __func__, x_size, y_size, x_pan, y_pan, x_offs, y_offs);

	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	unsigned short *fbbuff = NULL;
	int fh = -1, bp = 0;

	/* get the framebuffer device handle */
	fh = openFB(NULL);

	/* read current video mode */
	getVarScreenInfo(fh, &var); // framebuffer可变信息
	getFixScreenInfo(fh, &fix); // framebuffer固定信息

	if(x_offs > var.xres)
		x_offs = 0;
	if(y_offs > var.yres)
		y_offs = 0;

	/* blit buffer 2 fb */
	fbbuff = convertRGB2FB(fh, rgbbuff, x_size * y_size, var.bits_per_pixel, &bp);
	blit2FB(fh, fbbuff, alpha, x_size, y_size, var.xres, var.yres, x_pan, y_pan, x_offs, y_offs, bp);

	free(fbbuff);

	closeFB(fh);
}

int openFB(const char *name)
{
    Log(LOG_DEBUG, "function:%s --- paralist{name=%s}\n", __func__, name);

	int fh;
	char *dev;

	if (NULL == name) {
		dev = getenv("FRAMEBUFFER");
		if (dev)
			name = dev;
		else
			name = DEFAULT_FRAMEBUFFER;
	}

	if (-1 == (fh = open(name, O_RDWR))) {
		Log(LOG_ERROR, "function:%s --- %s\n", __func__, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return fh;
}

void closeFB(int fh)
{
    Log(LOG_DEBUG, "function:%s --- paralist{fh=%d}\n", __func__, fh);

	close(fh);
}

int clearFB(const char *name)
{
    Log(LOG_DEBUG, "function:%s --- paralist{name=%s}\n", __func__, name);

	int fh = -1;
	unsigned char *fb = NULL;
	struct fb_fix_screeninfo fix;

	if (-1 != (fh = openFB(name))) {
		getFixScreenInfo(fh, &fix);

        Log(LOG_DEBUG, "function:%s --- fix.smem_len=%u\n", __func__, fix.smem_len);

		if (MAP_FAILED == (fb = mmap(NULL, fix.smem_len, PROT_WRITE, MAP_SHARED, fh, 0))) {
			Log(LOG_WARN, "function:%s --- mmap %s\n", __func__, strerror(errno));
			closeFB(fh);
			return -1;
		}
	}

	// clear FB
	memset(fb, 0, fix.smem_len);

	munmap(fb, fix.smem_len);
	closeFB(fh);

	return 0;
}

void getVarScreenInfo(int fh, struct fb_var_screeninfo *var)
{
	if (ioctl(fh, FBIOGET_VSCREENINFO, var)) {

		Log(LOG_ERROR, "function:%s --- ioctl FBIOGET_VSCREENINFO %s\n", __func__, strerror(errno));

		exit(EXIT_FAILURE);
	}
}

void setVarScreenInfo(int fh, struct fb_var_screeninfo *var)
{
	if (ioctl(fh, FBIOPUT_VSCREENINFO, var)) {

		Log(LOG_ERROR, "function:%s --- ioctl FBIOPUT_VSCREENINFO %s\n", __func__, strerror(errno));

		exit(EXIT_FAILURE);
	}
}

void getFixScreenInfo(int fh, struct fb_fix_screeninfo *fix)
{
	if (ioctl(fh, FBIOGET_FSCREENINFO, fix)) {

		Log(LOG_ERROR, "function:%s --- ioctl FBIOGET_FSCREENINFO %s\n", __func__, strerror(errno));

		exit(EXIT_FAILURE);
	}
}

void make332map(struct fb_cmap *map)
{
	int rs, gs, bs, i;
	int r = 8, g = 8, b = 4;

	map->red = red;
	map->green = green;
	map->blue = blue;

	rs = 256 / (r - 1);
	gs = 256 / (g - 1);
	bs = 256 / (b - 1);

	for (i = 0; i < 256; i++) {
		map->red[i] = (rs * ((i / (g * b)) % r)) * 255;
		map->green[i] = (gs * ((i / b) % g)) * 255;
		map->blue[i] = (bs * ((i) % b)) * 255;
	}
}

void set8map(int fh, struct fb_cmap *map)
{
	if (ioctl(fh, FBIOPUTCMAP, map) < 0) {

		Log(LOG_ERROR, "function:%s --- Error putting colormap\n", __func__);

		exit(EXIT_FAILURE);
	}
}

void get8map(int fh, struct fb_cmap *map)
{
	if (ioctl(fh, FBIOGETCMAP, map) < 0) {

		Log(LOG_ERROR, "function:%s --- Error getting colormap\n", __func__);

		exit(EXIT_FAILURE);
	}
}

void set332map(int fh)
{
	make332map(&map332);
	set8map(fh, &map332);
}

void blit2FB(int fh, void *fbbuff, unsigned char *alpha, unsigned int pic_xs, unsigned int pic_ys, unsigned int scr_xs,
        unsigned int scr_ys, unsigned int xp, unsigned int yp, unsigned int xoffs, unsigned int yoffs, int cpp)
{
    Log(LOG_DEBUG,
		        "function:%s --- paralist{fh=%d,pic_xs=%u,pic_ys=%u,scr_xs=%u,scr_ys=%u,xp=%u,yp=%u,xoffs=%u,yoffs=%u,cpp=%d}\n",
		        __func__, fh, pic_xs, pic_ys, scr_xs, scr_ys, xp, yp, xoffs, yoffs, cpp);

	int i, xc, yc;
	unsigned char *fb;

	unsigned char *fbptr;
	unsigned char *imptr;

	xc = (pic_xs > scr_xs) ? scr_xs : pic_xs;
	yc = (pic_ys > scr_ys) ? scr_ys : pic_ys;

	if (MAP_FAILED == (fb = mmap(NULL, scr_xs * scr_ys * cpp, PROT_WRITE | PROT_READ, MAP_SHARED, fh, 0))) {

		Log(LOG_WARN, "function:%s --- mmap %s\n", __func__, strerror(errno));

		return;
	}

	if (cpp == 1) {
		// 对于色深为8位或8位以下的设备,在进行绘图操作前还需要设置合适的调色板
		get8map(fh, &map_back);
		set332map(fh);
	}

	fbptr = fb + (yoffs * scr_xs + xoffs) * cpp;
	imptr = fbbuff + (yp * pic_xs + xp) * cpp;

	if (alpha) {
		unsigned char * alphaptr;
		int from, to, x;

		alphaptr = alpha + (yp * pic_xs + xp);

		for (i = 0; i < yc; i++, fbptr += scr_xs * cpp, imptr += pic_xs * cpp, alphaptr += pic_xs) {
			for (x = 0; x < xc; x++) {
				int v;

				from = to = -1;
				for (v = x; v < xc; v++) {
					if (from == -1) {
						if (alphaptr[v] > 0x80)
							from = v;
					} else {
						if (alphaptr[v] < 0x80) {
							to = v;
							break;
						}
					}
				}
				if (from == -1)
					break;

				if (to == -1)
					to = xc;

				memcpy(fbptr + (from * cpp), imptr + (from * cpp), (to - from - 1) * cpp);
				x += to - from - 1;
			}
		}
	} else
		for (i = 0; i < yc; i++, fbptr += scr_xs * cpp, imptr += pic_xs * cpp)
			memcpy(fbptr, imptr, xc * cpp);

	if (cpp == 1)
		set8map(fh, &map_back);

	munmap(fb, scr_xs * scr_ys * cpp);
}

inline static unsigned char make8color(unsigned char r, unsigned char g, unsigned char b)
{
	return ((((r >> 5) & 7) << 5) | (((g >> 5) & 7) << 2) | ((b >> 6) & 3));
}

inline static unsigned short make15color(unsigned char r, unsigned char g, unsigned char b)
{
	return ((((r >> 3) & 31) << 10) | (((g >> 3) & 31) << 5) | ((b >> 3) & 31));
}

inline static unsigned short make16color(unsigned char r, unsigned char g, unsigned char b)
{
	return ((((r >> 3) & 31) << 11) | (((g >> 2) & 63) << 5) | ((b >> 3) & 31));
}

void* convertRGB2FB(int fh, unsigned char *rgbbuff, unsigned long count, int bpp, int *cpp)
{
	unsigned long i;
	void *fbbuff = NULL;
	u_int8_t *c_fbbuff;
	u_int16_t *s_fbbuff;
	u_int32_t *i_fbbuff;

	// 像素位深度
	switch (bpp) {
		case 8:
			*cpp = 1;
			c_fbbuff = (unsigned char *) malloc(count * sizeof(unsigned char));
			for (i = 0; i < count; i++)
				c_fbbuff[i] = make8color(rgbbuff[i * 3], rgbbuff[i * 3 + 1], rgbbuff[i * 3 + 2]);
			fbbuff = (void *) c_fbbuff;
			break;
		case 15:
			*cpp = 2;
			s_fbbuff = (unsigned short *) malloc(count * sizeof(unsigned short));
			for (i = 0; i < count; i++)
				s_fbbuff[i] = make15color(rgbbuff[i * 3], rgbbuff[i * 3 + 1], rgbbuff[i * 3 + 2]);
			fbbuff = (void *) s_fbbuff;
			break;
		case 16:
			*cpp = 2;
			s_fbbuff = (unsigned short *) malloc(count * sizeof(unsigned short));
			for (i = 0; i < count; i++)
				s_fbbuff[i] = make16color(rgbbuff[i * 3], rgbbuff[i * 3 + 1], rgbbuff[i * 3 + 2]);
			fbbuff = (void *) s_fbbuff;
			break;
		case 24:
		case 32:
			*cpp = 4;
			i_fbbuff = (unsigned int *) malloc(count * sizeof(unsigned int));
			for (i = 0; i < count; i++)
				i_fbbuff[i] = ((rgbbuff[i * 3] << 16) & 0xFF0000) | ((rgbbuff[i * 3 + 1] << 8) & 0xFF00)
				        | (rgbbuff[i * 3 + 2] & 0xFF);
			fbbuff = (void *) i_fbbuff;
			break;
		default: {

			Log(LOG_ERROR, "function:%s --- Unsupported video mode! You've got: %dbpp %s\n", __func__, bpp);

			exit(EXIT_FAILURE);
		}
	}

    Log(LOG_DEBUG, "function:%s --- paralist{fh=%d, count=%lu, bpp=%d, cpp=%d}\n", __func__, fh, count, bpp,
		        *cpp);

	return fbbuff;
}
