#ifndef __TELNETD_H__
#define __TELNETD_H__

#include "libtelnet.h"

typedef int (*telnet_printf_cb)(telnet_t *telnet, const char *fmt, ...);

typedef struct
{
	/* data */
	char *command;
	char *des;
	int (*cb)(telnet_t *telnet, telnet_printf_cb telnet_print, int argc, const char *argv[]);
} OPTISONS_T;

#endif
