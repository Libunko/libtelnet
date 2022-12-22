#ifndef __TELNET_CORE_H__
#define __TELNET_CORE_H__

#include "telnetd.h"

int telnet_core_init(int port, OPTISONS_T *options_t,  int options_len);
void usage(telnet_t *telnet, int tag_flag);

#endif
