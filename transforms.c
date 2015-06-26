#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "log.h"

#define DEBUG	(ALL_DEBUG_SWITCH && 1)

unsigned char * simple_resize(unsigned char * orgin, int ox, int oy, int dx, int dy)
{
    Log(LOG_DEBUG, "function:%s --- paralist{ox=%d,oy=%d,dx=%d,dy=%d}\n", __func__, ox, oy, dx, dy);

	unsigned char *cr, *p, *l;
	int i, j, k, ip;
	assert(cr = (unsigned char*) malloc(dx*dy*3));
	l = cr;

	for (j = 0; j < dy; j++, l += dx * 3) {
		p = orgin + (j * oy / dy * ox * 3);
		for (i = 0, k = 0; i < dx; i++, k += 3) {
			ip = i * ox / dx * 3;
			l[k] = p[ip];
			l[k + 1] = p[ip + 1];
			l[k + 2] = p[ip + 2];
		}
	}
	return (cr);
}

unsigned char * alpha_resize(unsigned char * alpha, int ox, int oy, int dx, int dy)
{
    Log(LOG_DEBUG, "function:%s --- paralist{ox=%d,oy=%d,dx=%d,dy=%d}\n", __func__, ox, oy, dx, dy);

	unsigned char *cr, *p, *l;
	int i, j, k;
	cr = (unsigned char*) malloc(dx * dy);
	l = cr;

	for (j = 0; j < dy; j++, l += dx) {
		p = alpha + (j * oy / dy * ox);
		for (i = 0, k = 0; i < dx; i++)
			l[k++] = p[i * ox / dx];
	}

	return (cr);
}

unsigned char * color_average_resize(unsigned char * orgin, int ox, int oy, int dx, int dy)
{
    Log(LOG_DEBUG, "function:%s --- paralist{ox=%d,oy=%d,dx=%d,dy=%d}\n", __func__, ox, oy, dx, dy);

	unsigned char *cr, *p, *q;
	int i, j, k, l;
	int xa, xb, ya, yb;
	int sq, r, g, b;
	assert(cr = (unsigned char*) malloc(dx*dy*3));
	p = cr;

	for (j = 0; j < dy; j++) {
		for (i = 0; i < dx; i++, p += 3) {
			// first point in origin image
			xa = (i + 0.5) * ox / dx - 0.5;
			ya = (j + 0.5) * oy / dy - 0.5;

			// second point in origin image
			xb = xa + 1; if(xb > ox) xb = ox;
			yb = ya;

			for (l = ya, r = 0, g = 0, b = 0, sq = 0; l <= yb; l++) {
				q = orgin + ((l * ox + xa) * 3);
				for (k = xa; k <= xb; k++, q += 3, sq++) {
					r += q[0];
					g += q[1];
					b += q[2];
				}
			}
			// 4 points average value
			p[0] = r / sq;
			p[1] = g / sq;
			p[2] = b / sq;
		}
	}

	return (cr);
}
