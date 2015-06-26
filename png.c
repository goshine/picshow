/*
 fbv  --  simple image viewer for the linux framebuffer
 Copyright (C) 2000, 2001, 2003  Mateusz Golicz

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

#include <png.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "fbv.h"
#include "log.h"

#define DEBUG (ALL_DEBUG_SWITCH && 1)

int fh_png_id(char *name)
{
    Log(LOG_DEBUG, "function:%s --- paralist:name=%s\n", __func__, name);

	int fd;
	char id[4];

	if (-1 == (fd = open(name, O_RDONLY))) {
		Log(LOG_WARN, "function:%s --- open %s\n", __func__, strerror(errno));
		return (0);
	}

	if(0 >= read(fd, id, 4)){
		Log(LOG_WARN, "function:%s --- read %s\n", __func__, strerror(errno));
		return (0);
	}
	close(fd);

	if (id[1] == 'P' && id[2] == 'N' && id[3] == 'G')
		return (1);

	Log(LOG_WARN, "[%s WARN] function:%s --- file is not png image\n", __func__);
	return (0);
}

int fh_png_load(char *name, unsigned char *buffer, unsigned char ** alpha, int x, int y)
{
    Log(LOG_DEBUG, "function:%s --- paralist:name=%s,x=%d,y=%d\n", __func__, name, x, y);

	png_bytep rptr[2];
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	FILE *fh = NULL;
	int i;
	int bit_depth, color_type, interlace_type;
	int number_passes, pass, trans = 0;
	char *rp = NULL;
	char *fbptr = NULL;

	if (!(fh = fopen(name, "rb"))) {
		Log(LOG_WARN, "function:%s --- %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}

	if (NULL == (png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		Log(LOG_WARN, "function:%s --- png file format error!\n", __func__);
		return (FH_ERROR_FORMAT);
	}

	if (NULL == (info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		fclose(fh);
		Log(LOG_WARN, "function:%s --- png file format error!\n", __func__);
		return (FH_ERROR_FORMAT);
	}

	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		if (rp)
			free(rp);
		fclose(fh);
		Log(LOG_WARN, "function:%s --- png file format error!\n", __func__);
		return (FH_ERROR_FORMAT);
	}

	png_init_io(png_ptr, fh);
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);

	if (bit_depth < 8)
		png_set_packing(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		trans = 1;
		png_set_tRNS_to_alpha(png_ptr);
	}

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	number_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA || color_type == PNG_COLOR_TYPE_RGB_ALPHA || trans) {
		unsigned char * alpha_buffer = NULL;
		unsigned char * aptr = NULL;

		if (NULL == (alpha_buffer = (unsigned char*) malloc(width * height))) {
			Log(LOG_WARN, "function:%s --- alpha buffer malloc failed!\n", __func__);
			free(rp);
			fclose(fh);
			return (FH_ERROR_FORMAT);
		}

		if (NULL == (rp = (char*) malloc(width * 4))) {
			Log(LOG_WARN, "function:%s --- rp malloc failed!\n", __func__);
			free(rp);
			fclose(fh);
			return (FH_ERROR_FORMAT);
		}
		rptr[0] = (png_bytep) rp;

		*alpha = alpha_buffer;

		for (pass = 0; pass < number_passes; pass++) {
			fbptr = (char *) buffer;
			aptr = alpha_buffer;

			for (i = 0; i < height; i++) {
				int n;
				unsigned char *trp = (unsigned char *) rp;

				png_read_rows(png_ptr, rptr, NULL, 1);

				for (n = 0; n < width; n++, fbptr += 3, trp += 4) {
					memcpy(fbptr, trp, 3);
					*(aptr++) = trp[3];
				}
			}
		}

		free(rp);
	} else {
		for (pass = 0; pass < number_passes; pass++) {
			fbptr = (char *) buffer;
			for (i = 0; i < height; i++, fbptr += width * 3) {
				rptr[0] = (png_bytep) fbptr;
				png_read_rows(png_ptr, rptr, NULL, 1);
			}
		}
	}

	png_read_end(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);

	fclose(fh);

	return (FH_ERROR_OK);
}

int fh_png_getsize(char *name, int *x, int *y)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	FILE *fh = NULL;
	int bit_depth, color_type, interlace_type;
	char *rp = NULL;

	if (!(fh = fopen(name, "rb"))) {
		Log(LOG_WARN, "function:%s --- %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}

	if (NULL == (png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		Log(LOG_WARN, "function:%s --- png file format error!\n", __func__);
		return (FH_ERROR_FORMAT);
	}

	if (NULL == (info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		fclose(fh);
		Log(LOG_WARN, "function:%s --- png file format error!\n", __func__);
		return (FH_ERROR_FORMAT);
	}

	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		if (rp)
			free(rp);
		fclose(fh);
		Log(LOG_WARN, "function:%s --- png file format error!\n", __func__);
		return (FH_ERROR_FORMAT);
	}

	png_init_io(png_ptr, fh);
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
	*x = width;
	*y = height;

	fclose(fh);

	Log(LOG_INFO, "function:%s --- paralist:name=%s,x=%d,y=%d\n", __func__, name, *x, *y);

	return (FH_ERROR_OK);
}
