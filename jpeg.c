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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "fbv.h"
#include "log.h"

#define DEBUG (ALL_DEBUG_SWITCH && 1)

struct r_jpeg_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf envbuffer;
};

int fh_jpeg_id(char *name)
{
    Log(LOG_DEBUG, "function:%s --- paralist{name=%s}\n", __func__, name);

	int fd;
	unsigned char id[10];

	if (-1 == (fd = open(name, O_RDONLY))) {
		Log(LOG_WARN, "function:%s --- open %s\n", __func__, strerror(errno));
		return (0);
	}

	if(0 >= read(fd, id, 10)){
		Log(LOG_WARN, "function:%s --- read %s\n", __func__, strerror(errno));
		return (0);
	}
	close(fd);

	if (id[6] == 'J' && id[7] == 'F' && id[8] == 'I' && id[9] == 'F')
		return (1);
	if (id[0] == 0xff && id[1] == 0xd8 && id[2] == 0xff)
		return (1);

	Log(LOG_WARN, "function:%s --- file is not jpg image\n", __func__);
	return (0);
}

void jpeg_cb_error_exit(j_common_ptr cinfo)
{
	struct r_jpeg_error_mgr *mptr;
	mptr = (struct r_jpeg_error_mgr*) cinfo->err;

	(*cinfo->err->output_message)(cinfo);
	longjmp(mptr->envbuffer, 1);
}

int fh_jpeg_load(char *filename, unsigned char *buffer, unsigned char ** alpha, int x, int y)
{
    Log(LOG_DEBUG, "function:%s --- paralist{filename=%s,x=%d,y=%d}\n", __func__, filename, x, y);

	struct jpeg_decompress_struct cinfo;
	struct jpeg_decompress_struct *ciptr;
	struct r_jpeg_error_mgr emgr;
	unsigned char *bp;
	int px, c;
	FILE *fh;
	JSAMPLE *lb;

	ciptr = &cinfo;
	if (!(fh = fopen(filename, "rb")))
		return (FH_ERROR_FILE);
	ciptr->err = jpeg_std_error(&emgr.pub);
	emgr.pub.error_exit = jpeg_cb_error_exit;
	if (setjmp(emgr.envbuffer) == 1) {
		// FATAL ERROR - Free the object and return...
		jpeg_destroy_decompress(ciptr);
		fclose(fh);
		Log(LOG_WARN, "function:%s --- jpg file format error!\n", __func__);
		return (FH_ERROR_FORMAT);
	}

	jpeg_create_decompress(ciptr);
	jpeg_stdio_src(ciptr, fh);
	jpeg_read_header(ciptr, TRUE);
	ciptr->out_color_space = JCS_RGB;
	jpeg_start_decompress(ciptr);

	px = ciptr->output_width;
	c = ciptr->output_components;

	if (c == 3) {
		lb = (*ciptr->mem->alloc_small)((j_common_ptr) ciptr, JPOOL_PERMANENT, c * px);
		bp = buffer;
		while (ciptr->output_scanline < ciptr->output_height) {
			jpeg_read_scanlines(ciptr, &lb, 1);
			memcpy(bp, lb, px * c);
			bp += px * c;
		}

	}

	jpeg_finish_decompress(ciptr);
	jpeg_destroy_decompress(ciptr);

	fclose(fh);

	return (FH_ERROR_OK);
}

int fh_jpeg_getsize(char *filename, int *x, int *y)
{
	struct jpeg_decompress_struct cinfo;
	struct r_jpeg_error_mgr emgr;
	FILE *fh;

	if (!(fh = fopen(filename, "rb"))) {
		Log(LOG_WARN, "function:%s --- %s\n", __func__, strerror(errno));
		return (FH_ERROR_FILE);
	}

	cinfo.err = jpeg_std_error(&emgr.pub);
	emgr.pub.error_exit = jpeg_cb_error_exit;
	if (setjmp(emgr.envbuffer) == 1) {
		// FATAL ERROR - Free the object and return...
		jpeg_destroy_decompress(&cinfo);
		fclose(fh);
		Log(LOG_WARN, "function:%s --- jpg file format error!\n", __func__);
		return (FH_ERROR_FORMAT);
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fh);
	jpeg_read_header(&cinfo, TRUE);
	cinfo.out_color_space = JCS_RGB;
	jpeg_start_decompress(&cinfo);

	*x = cinfo.output_width;
	*y = cinfo.output_height;

	jpeg_destroy_decompress(&cinfo);

	fclose(fh);

	Log(LOG_INFO, "function:%s --- paralist:name=%s,x=%d,y=%d\n", __func__, filename, *x, *y);

	return (FH_ERROR_OK);
}
