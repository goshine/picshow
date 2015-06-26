/*
 fbv  --  simple image viewer for the linux framebuffer
 Copyright (C) 2002  Tomasz Sterna

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "fbv.h"
#include "log.h"

#define DEBUG (ALL_DEBUG_SWITCH && 1)

#define BMP_TORASTER_OFFSET	10
#define BMP_SIZE_OFFSET		18
#define BMP_BPP_OFFSET		28
#define BMP_RLE_OFFSET		30
#define BMP_COLOR_OFFSET	54

#define fill4B(a)	( ( 4 - ( (a) % 4 ) ) & 0x03)

struct color
{
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};

int fh_bmp_id(char *name)
{
    Log(LOG_DEBUG, "function:%s --- paralist:name=%s\n", __func__, name);

	int fd;
	char id[2];

	if (-1 == (fd = open(name, O_RDONLY))) {
		Log(LOG_WARN, "function:%s --- %s\n", __func__, strerror(errno));
		return (0);
	}

	if(0 >= read(fd, id, 2)){
		Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
		return 0;
	}
	close(fd);

	if (id[0] == 'B' && id[1] == 'M')
		return (1);

	Log(LOG_WARN, "function:%s --- file is not png image\n", __func__);
	return (0);
}

// 获取调色板信息
void fetch_pallete(int fd, struct color pallete[], int count)
{
    Log(LOG_DEBUG, "function:%s --- paralist:fd=%d, count=%d\n", __func__, fd, count);

	int i;
	int readbytes = count << 2;
	char *buf = (char *) malloc(sizeof(char) * readbytes);

	if (NULL == buf) {
		Log(LOG_ERROR, "function:%s --- pallete buffer malloc failed!\n", __func__);

		exit(EXIT_FAILURE);
	}

	lseek(fd, BMP_COLOR_OFFSET, SEEK_SET);
	if(0 >= read(fd, buf, readbytes)){
		free(buf);
		Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < readbytes; i += 4) {
		pallete[i].red = buf[i + 2];
		pallete[i].green = buf[i + 1];
		pallete[i].blue = buf[i];
	}

	free(buf);
	return;
}

int fh_bmp_load(char *name, unsigned char *buffer, unsigned char **alpha, int x, int y)
{
    Log(LOG_DEBUG, "function:%s --- paralist:name=%s, x=%d, y=%d\n", __func__, name, x, y);

	int fd, bpp, raster, i, j, k, skip;
	unsigned char buff[4];
	unsigned char *wr_buffer = buffer + x * (y - 1) * 3;
	struct color pallete[256];

	if (-1 == (fd = open(name, O_RDONLY))) {
		Log(LOG_WARN, "function:%s --- %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}

	//  get bmp data start position
	if (lseek(fd, BMP_TORASTER_OFFSET, SEEK_SET) == -1) {
		Log(LOG_WARN, "function:%s --- %s\n", __func__, strerror(errno));
		return (FH_ERROR_FORMAT);
	}
	if(0 >= read(fd, buff, 4)){
		Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}
	raster = buff[0] + (buff[1] << 8) + (buff[2] << 16) + (buff[3] << 24);

	// get bmp bit per pixel
	if (lseek(fd, BMP_BPP_OFFSET, SEEK_SET) == -1) {
		return (FH_ERROR_FORMAT);
	}
	if(0 >= read(fd, buff, 2)){
		Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}
	bpp = buff[0] + (buff[1] << 8);

	switch (bpp) {
		case 1: {/* monochrome */
			int linebytes = x >> 3;
			char *buf = (char *) malloc(sizeof(char) * linebytes);
			if (NULL == buf) {
				Log(LOG_WARN, "function:%s --- 1bit buffer malloc failed!\n", __func__);
				return (FH_ERROR_FORMAT);
			}

			skip = fill4B(linebytes+(x%8?1:0));
			lseek(fd, raster, SEEK_SET);

			for (i = 0; i < y; i++) {
				if(0 >= read(fd, buf, linebytes)){
					Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
					return (FH_ERROR_FILE);
				}

				for (j = 0; j < linebytes; j++) {
					for (k = 0; k < 8; k++) {
						if (*(buf + j) & 0x80) {
							*wr_buffer++ = 0xff;
							*wr_buffer++ = 0xff;
							*wr_buffer++ = 0xff;
						} else {
							*wr_buffer++ = 0x00;
							*wr_buffer++ = 0x00;
							*wr_buffer++ = 0x00;
						}
						*(buf + j) = *(buf + j) << 1;
					}

				}
				if (x % 8) {
					for (k = 0; k < x % 8; k++) {
						if (*(buf + j) & 0x80) {
							*wr_buffer++ = 0xff;
							*wr_buffer++ = 0xff;
							*wr_buffer++ = 0xff;
						} else {
							*wr_buffer++ = 0x00;
							*wr_buffer++ = 0x00;
							*wr_buffer++ = 0x00;
						}
						*(buf + j) = *(buf + j) << 1;
					}

				}
				if (skip){
					if(0 >= read(fd, buf, skip)){
						Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
						return (FH_ERROR_FILE);
					}
				}

				wr_buffer -= x * 6; /* backoff 2 lines - x*2 *3 */
			}
			free(buf);
			break;
		}
		case 4: {/* 4bit palletized */
			int linebytes = x >> 1;
			unsigned char color1, color2;
			char *buf = (char *) malloc(sizeof(char) * linebytes);
			if (NULL == buf) {
				Log(LOG_WARN, "function:%s --- 4bits buffer malloc failed!\n", __func__);
				return (FH_ERROR_FORMAT);
			}

			skip = fill4B(linebytes+x%2);
			fetch_pallete(fd, pallete, 16);
			lseek(fd, raster, SEEK_SET);

			for (i = 0; i < y; i++) {
				if(0 >= read(fd, buf, linebytes)){
					Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
					return (FH_ERROR_FILE);
				}
				for (j = 0; j < linebytes; j++) {
					color1 = *(buf + j) >> 4;
					color2 = *(buf + j) & 0x0f;
					*wr_buffer++ = pallete[color1].red;
					*wr_buffer++ = pallete[color1].green;
					*wr_buffer++ = pallete[color1].blue;
					*wr_buffer++ = pallete[color2].red;
					*wr_buffer++ = pallete[color2].green;
					*wr_buffer++ = pallete[color2].blue;
				}
				if (x % 2) {
					color1 = *(buf + j) >> 4;
					*wr_buffer++ = pallete[color1].red;
					*wr_buffer++ = pallete[color1].green;
					*wr_buffer++ = pallete[color1].blue;
				}
				if (skip){
					if(0 >= read(fd, buf, skip)){
						Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
						return (FH_ERROR_FILE);
					}
				}

				wr_buffer -= x * 6; /* backoff 2 lines - x*2 *3 */
			}
			free(buf);
			break;
		}
		case 8: {/* 8bit palletized */
			unsigned char *buf = (unsigned char *) malloc(sizeof(unsigned char) * x);
			if (NULL == buf) {
				Log(LOG_WARN, "function:%s --- 8bits buffer malloc failed!\n", __func__);
				return (FH_ERROR_FORMAT);
			}

			skip = fill4B(x);
			fetch_pallete(fd, pallete, 256);
			lseek(fd, raster, SEEK_SET);

			for (i = 0; i < y; i++) {
				if(0 >= read(fd, buf, x)){
					Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
					return (FH_ERROR_FILE);
				}
				for (j = 0; j < x; j++) {
					*wr_buffer++ = pallete[*(buf + j)].red;
					*wr_buffer++ = pallete[*(buf + j)].green;
					*wr_buffer++ = pallete[*(buf + j)].blue;
				}
				if (skip){
					if(0 >= read(fd, buf, skip)){
						Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
						return (FH_ERROR_FILE);
					}
				}

				wr_buffer -= x * 6; /* backoff 2 lines - x*2 *3 */
			}
			free(buf);
			break;
		}
		case 16: {/* 16bit RGB */
			return (FH_ERROR_FORMAT);
			break;
		}
		case 24: {/* 24bit RGB */
			int linebytes = x * 3;
			char *buf = (char *) malloc(sizeof(char) * linebytes);
			if (NULL == buf) {
				Log(LOG_WARN, "function:%s --- 24bits buffer malloc failed!\n", __func__);
				return (FH_ERROR_FORMAT);
			}

			skip = fill4B(linebytes);
			lseek(fd, raster, SEEK_SET);

			for (i = 0; i < y; i++) {
				if(0 >= read(fd, buf, linebytes)){
					Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
					return (FH_ERROR_FILE);
				}
				for (j = 0, k = 0; j < x; j++, k += 3) {
					*wr_buffer++ = *(buf + k + 2); // R
					*wr_buffer++ = *(buf + k + 1); // G
					*wr_buffer++ = *(buf + k); // B
				}
				if (skip){
					if(0 >= read(fd, buf, skip)){
						Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
						return (FH_ERROR_FILE);
					}
				}

				wr_buffer -= (linebytes << 1); /* backoff 2 lines - x*2 *3 */
			}
			free(buf);
			break;
		}
		default:
			return (FH_ERROR_FORMAT);
	}

	close(fd);

	return (FH_ERROR_OK);
}
int fh_bmp_getsize(char *name, int *x, int *y)
{
	int fd;
	unsigned char size[4];

	if (-1 == (fd = open(name, O_RDONLY))) {
		Log(LOG_WARN, "function:%s --- open %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}

	if (lseek(fd, BMP_SIZE_OFFSET, SEEK_SET) == -1) {
		Log(LOG_WARN, "function:%s --- lseek %s\n", __func__, strerror(errno));
		return (FH_ERROR_FORMAT);
	}

	if(0 >= read(fd, size, 4)){
		Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}
	*x = size[0] + (size[1] << 8) + (size[2] << 16) + (size[3] << 24);

	if(0 >= read(fd, size, 4)){
		Log(LOG_WARN, "function:%s ---read %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}
	*y = size[0] + (size[1] << 8) + (size[2] << 16) + (size[3] << 24);

	close(fd);

	Log(LOG_INFO, "function:%s --- paralist:name=%s,x=%d,y=%d\n", __func__, name, *x, *y);

	return (FH_ERROR_OK);
}
