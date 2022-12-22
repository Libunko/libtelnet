#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "telnetd.h"
#include "telnet-core.h"

#define TELNETD_PORT	23233

static int isTelnetd;

static int help(telnet_t *telnet, telnet_printf_cb telnet_print, int argc, const char *argv[])
{
	usage(telnet, 0);
	return 0;
}

static int quit(telnet_t *telnet, telnet_printf_cb telnet_print, int argc, const char *argv[])
{
	return -1;
}

static int test(telnet_t *telnet, telnet_printf_cb telnet_print, int argc, const char *argv[])
{
	for (size_t i = 0; i < argc; i++)
		telnet_print(telnet, "argv[%d] %s\n", i, argv[i]);

	telnet_print(telnet, "result %d\n", 0);
	return 0;
}

static OPTISONS_T telnet_options_t[] = 
{
	{.command = "help",	.des = NULL,			help},
	{.command = "quit",	.des = NULL,			quit},
	{.command = "test",	.des = "[options]",		test},
};

static void *do_telnet_core_init(void *argv)
{
	telnet_core_init(TELNETD_PORT, telnet_options_t, sizeof(telnet_options_t) / sizeof(telnet_options_t[0]));
	isTelnetd = 0;
	return NULL;
}

void start_telnetd(void)
{
	pthread_t pth = -1;

	if (isTelnetd)
	{
		printf("already in checking\n");
		return;
	}

	isTelnetd = 1;
	if (pthread_create(&pth, NULL, do_telnet_core_init, NULL))
	{
		isTelnetd = 0;
		printf("pthread_create error\n");
		return;
	}

	if (pthread_detach(pth))
	{
		printf("pthread_detach %ld error\n", pth);
	}
}

int main(int argc, const char *argv[])
{
	start_telnetd();

	while (1)
	{
		sleep(1);
	}
}
