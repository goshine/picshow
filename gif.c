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
#include <setjmp.h>
#include <gif_lib.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "fbv.h"
#include "log.h"

#define DEBUG (ALL_DEBUG_SWITCH && 1)

#define gflush return(FH_ERROR_FILE);
#define grflush { DGifCloseFile(gft); return(FH_ERROR_FORMAT); }
#define mgrflush { free(lb); free(slb); DGifCloseFile(gft); return(FH_ERROR_FORMAT); }

extern int show_image(struct image *img, int delay);

// 返回1，该文件是GIF文件，否则返回0
int fh_gif_id(char *name)
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

	// GIF图像文件，头三个字符为GIF
	if (id[0] == 'G' && id[1] == 'I' && id[2] == 'F')
		return (1);

	Log(LOG_WARN, "function:%s --- file is not gif image\n", __func__);
	return (0);
}

inline void m_rend_gif_decodecolormap(unsigned char *cmb, unsigned char *rgbb, ColorMapObject *cm, int s, int l,
        int transparency)
{
	GifColorType *cmentry;
	int i;

	for (i = 0; i < l; i++) {
		cmentry = &cm->Colors[cmb[i]];
		*(rgbb++) = cmentry->Red;
		*(rgbb++) = cmentry->Green;
		*(rgbb++) = cmentry->Blue;
	}
}

/* Thanks goes here to Mauro Meneghin, who implemented interlaced GIF files support */

int fh_gif_load(char *name, unsigned char *buf, unsigned char ** alpha, int x, int y)
{
    Log(LOG_DEBUG, "function:%s --- paralist:name=%s,x=%d,y=%d\n", __func__, name, x, y);

	int in_nextrow[4] = { 8, 8, 4, 2 }; //interlaced jump to the row current+in_nextrow
	int in_beginrow[4] = { 0, 4, 2, 1 }; //begin pass j from that row number
	int transparency = -1; //-1 means not transparency present
	int px, py, i, j;
	int cmaps;
	int extcode;
	char *fbptr;
	char *lb;
	char *slb;
	GifFileType *gft;
	GifByteType *extension;
	GifRecordType rt;
	ColorMapObject *cmap;

	int delay = 0;
	//int error = 0;
	struct image *img = NULL;
	unsigned char *buffer = NULL;

	//if(NULL == (gft=DGifOpenFileName(name, &error))){
	if (NULL == (gft = DGifOpenFileName(name))) {
		Log(LOG_WARN, "function:%s --- gif file open failed!\n", __func__);
		gflush;
	}

	if (NULL == (img = (struct image *) malloc(sizeof(struct image)))) {
		Log(LOG_WARN, "function:%s --- gif malloc failed!\n", __func__);
		grflush;
	}

	do {
		if (DGifGetRecordType(gft, &rt) == GIF_ERROR) {
			Log(LOG_WARN, "function:%s --- gif get record type failed!\n", __func__);
			grflush
		}

		switch (rt) {
			case IMAGE_DESC_RECORD_TYPE:
				if (DGifGetImageDesc(gft) == GIF_ERROR) {
					Log(LOG_WARN, "function:%s --- gif get desc failed!\n", __func__);
					grflush
				}

				px = gft->Image.Width;
				py = gft->Image.Height;

				Log(LOG_INFO, "function:%s --- paralist:name=%s,x=%d,y=%d\n", __func__, name, px, py);

				lb = (char*) malloc(px * 3);
				slb = (char*) malloc(px);
				buffer = (unsigned char *) malloc(px * py * 3);

				if (lb != NULL && slb != NULL && buffer != NULL) {
					unsigned char *alphaptr = NULL;

					cmap = (gft->Image.ColorMap ? gft->Image.ColorMap : gft->SColorMap);
					cmaps = cmap->ColorCount;

					fbptr = (char*) buffer;

					if (transparency != -1) {
						if (NULL == (alphaptr = malloc(px * py))) {
							Log(LOG_WARN, "function:%s --- alphaptr malloc failed!\n", __func__);
						}
						*alpha = alphaptr;
					}

					if (!(gft->Image.Interlace)) {
						for (i = 0; i < py; i++, fbptr += px * 3) {
							int j;

							if (DGifGetLine(gft, (GifPixelType*) slb, px) == GIF_ERROR) {
								Log(LOG_WARN, "function:%s --- gif get line failed!\n", __func__);
								mgrflush;
							}

							m_rend_gif_decodecolormap((unsigned char*) slb, (unsigned char*) lb, cmap, cmaps, px,
							        transparency);
							memcpy(fbptr, lb, px * 3);

							if (alphaptr)
								for (j = 0; j < px; j++) {
									*(alphaptr++) = (((unsigned char*) slb)[j] == transparency) ? 0x00 : 0xff;
								}
						}
					} else {
						unsigned char * aptr = NULL;

						for (j = 0; j < 4; j++) {
							int k;

							if (alphaptr)
								aptr = alphaptr + (in_beginrow[j] * px);

							fbptr = (char*) buffer + (in_beginrow[j] * px * 3);

							for (i = in_beginrow[j]; i < py;
							        i += in_nextrow[j], fbptr += px * 3 * in_nextrow[j], aptr += px * in_nextrow[j]) {
								if (DGifGetLine(gft, (GifPixelType*) slb, px) == GIF_ERROR) {
									Log(LOG_WARN, "function:%s --- gif get line failed!\n", __func__);
									mgrflush
								}

								m_rend_gif_decodecolormap((unsigned char*) slb, (unsigned char*) lb, cmap, cmaps, px,
								        transparency);
								memcpy(fbptr, lb, px * 3);

								if (alphaptr)
									for (k = 0; k < px; k++) {
										aptr[k] = (((unsigned char*) slb)[k] == transparency) ? 0x00 : 0xff;
									}
							}
						}
					}

					img->rgb = buffer;
					img->alpha = *alpha;
					img->width = px;
					img->height = py;
					img->do_free = 0;

					show_image(img, delay);

					free(img->rgb);
					free(img->alpha);
				} else {
					Log(LOG_WARN, "function:%s --- gif malloc failed!\n", __func__);
				}

				if (lb) {
					free(lb);
					lb = NULL;
				}
				if (slb) {
					free(slb);
					slb = NULL;
				}

				break;
			case EXTENSION_RECORD_TYPE:
				if (DGifGetExtension(gft, &extcode, &extension) == GIF_ERROR) {
					Log(LOG_WARN, "function:%s --- gif get extension failed!\n", __func__);
					grflush
				}

				if (extcode == GRAPHICS_EXT_FUNC_CODE && extension[0] == 4) {
					delay = (extension[3] << 8 | extension[2]) * 10;
				}

				while (extension != NULL) {
					if (DGifGetExtensionNext(gft, &extension) == GIF_ERROR) {
						Log(LOG_WARN, "function:%s --- gif get next extension failed!\n", __func__);
						grflush
					}
				}
				break;
			default:
				break;
		}
	} while (rt != TERMINATE_RECORD_TYPE);

	DGifCloseFile(gft);

	free(img);

	return (FH_ERROR_OK);
}
