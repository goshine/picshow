/*
 * led_conf.h
 *
 *  Created on: Dec 25, 2013
 *      Author: jqc
 */

#ifndef __LED_CONF_H__
#define __LED_CONF_H__

#define CONF_PATH			"/opt/coloro/app.config"
#define BUF_SIZE			256
#define CONF_KEY_LED_SIZE	"resolution"

int get_led_size(const char *confpath, int *x, int *y);

#endif /* __LED_CONF_H__ */
