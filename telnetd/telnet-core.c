/*
 * Sean Middleditch
 * sean@sourcemud.org
 *
 * The author or authors of this code dedicate any and all copyright interest
 * in this code to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and successors. We
 * intend this dedication to be an overt act of relinquishment in perpetuity of
 * all present and future rights to this code under copyright law. 
 */

#if !defined(_WIN32)
// #	if !defined(_BSD_SOURCE)
// #		define _BSD_SOURCE
// #	endif

#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#	include <poll.h>
#	include <unistd.h>

#	define SOCKET int
#else
#	include <winsock2.h>
#	include <ws2tcpip.h>

#ifndef _UCRT
#	define snprintf _snprintf
#endif

#	define poll WSAPoll
#	define close closesocket
#	define strdup _strdup
#	if !defined(ECONNRESET)
#		define ECONNRESET WSAECONNRESET
#	endif
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "libtelnet.h"
#include "telnet-core.h"

#define MAX_USERS 64
#define LINEBUFFER_SIZE 256
#define ECHO_TAG telnet_printf(telnet, "==> ")

static const telnet_telopt_t telopts[] = {
	{ TELNET_TELOPT_COMPRESS2,	TELNET_WILL, TELNET_DONT },
	{ -1, 0, 0 }
};

struct user_t {
	char *name;
	SOCKET sock;
	telnet_t *telnet;
	char linebuf[256];
	int linepos;
};

static struct user_t users[MAX_USERS];
static OPTISONS_T *telnet_options_t;
static int telnet_options_len;

static void _message(const char *from, const char *msg) {
	int i;
	for (i = 0; i != MAX_USERS; ++i) {
		if (users[i].sock != -1) {
			telnet_printf(users[i].telnet, "%s: %s\n", from, msg);
		}
	}
}

static void _send(SOCKET sock, const char *buffer, size_t size) {
	int rs;

	/* ignore on invalid socket */
	if (sock == -1)
		return;

	/* send data */
	while (size > 0) {
		if ((rs = send(sock, buffer, (int)size, 0)) == -1) {
			if (errno != EINTR && errno != ECONNRESET) {
				fprintf(stderr, "send() failed: %s\n", strerror(errno));
				exit(1);
			} else {
				return;
			}
		} else if (rs == 0) {
			fprintf(stderr, "send() unexpectedly returned 0\n");
			exit(1);
		}

		/* update pointer and size to see if we've got more to send */
		buffer += rs;
		size -= rs;
	}
}

static int setargs(char *args, char **argv)
{
	int count = 0;

	while (isspace(*args))
		++args;
	while (*args)
	{
		if (argv)
			argv[count] = args;
		while (*args && !isspace(*args))
			++args;
		if (argv && *args)
			*args++ = '\0';
		while (isspace(*args))
			++args;
		count++;
	}
	return count;
}

static char **parsedargs(char *args, int *argc)
{
	char **argv = NULL;
	int argn = 0;

	if (args && *args && (args = strdup(args)) && (argn = setargs(args, NULL)) && (argv = malloc((argn + 1) * sizeof(char *))))
	{
		*argv++ = args;
		argn = setargs(args, argv);
	}

	if (args && !argv)
		free(args);

	*argc = argn;
	return argv;
}

static void freeparsedargs(char **argv)
{
	if (argv)
	{
		free(argv[-1]);
		free(argv - 1);
	}
}

void usage(telnet_t *telnet, int tag_flag)
{
	int i;

	telnet_printf(telnet, "\nUsage: <cmd> <args>\n");
	for (i = 0; i < telnet_options_len; i++)
	{
		telnet_printf(telnet, " %s %s\n", telnet_options_t[i].command, telnet_options_t[i].des ? telnet_options_t[i].des : "");
	}

	if (tag_flag)
		ECHO_TAG;
}

static void do_handler(telnet_t *telnet, struct user_t *user, const char *buffer, size_t size)
{
	int i;
	char cmd[256] = {0};
	char **argv = NULL;
	int argc;
	char *tmp = NULL;
	int len = 0;

	tmp = strchr(buffer, ' ');
	if (NULL == tmp)
	{
		tmp = strchr(buffer, '\r');
	}

	len = tmp - buffer;

	strncpy(cmd, buffer, sizeof(cmd) - len > 0 ? len : (sizeof(cmd) -1));

	for (i = 0; i < telnet_options_len; i++)
	{
		if (0 == strncmp(cmd, telnet_options_t[i].command, strlen(telnet_options_t[i].command)))
		{
			if (telnet_options_t[i].cb)
			{
				argv = parsedargs((char *)buffer, &argc);
				if (telnet_options_t[i].cb(telnet, telnet_printf, argc, (const char **)argv))
				{
					close(user->sock);
					user->sock = -1;
					telnet_free(user->telnet);
					return;
				}
				freeparsedargs(argv);
			}
			else
			{
				telnet_printf(telnet, "command %s callback not found\n", cmd);
			}
			ECHO_TAG;
			break;
		}
	}
	if (i == telnet_options_len)
	{
		telnet_printf(telnet, "command %s not found\n", cmd);
		usage(telnet, 1);
	}
}

static void _event_handler(telnet_t *telnet, telnet_event_t *ev,
		void *user_data) {
	struct user_t *user = (struct user_t*)user_data;

	switch (ev->type) {
	/* data received */
	case TELNET_EV_DATA:
		/* \r\n */
		if (2 == ev->data.size)
		{
			usage(user->telnet, 1);
		}
		else
		{
			do_handler(telnet, user, ev->data.buffer, ev->data.size - 2);
		}
		break;
	/* data must be sent */
	case TELNET_EV_SEND:
		_send(user->sock, ev->data.buffer, ev->data.size);
		break;
	/* enable compress2 if accepted by client */
	case TELNET_EV_DO:
		if (ev->neg.telopt == TELNET_TELOPT_COMPRESS2)
			telnet_begin_compress2(telnet);
		break;
	/* error */
	case TELNET_EV_ERROR:
		close(user->sock);
		user->sock = -1;
		if (user->name != 0) {
			_message(user->name, "** HAS HAD AN ERROR **");
			free(user->name);
			user->name = 0;
		}
		telnet_free(user->telnet);
		break;
	default:
		/* ignore */
		break;
	}
}

int telnet_core_init(int port, OPTISONS_T *options_t,  int options_len) {
	char buffer[512];
	short listen_port;
	SOCKET listen_sock;
	SOCKET client_sock;
	int rs;
	int i;
	struct sockaddr_in addr;
	socklen_t addrlen;
	struct pollfd pfd[MAX_USERS + 1];

	/* initialize Winsock */
#if defined(_WIN32)
	WSADATA wsd;
	WSAStartup(MAKEWORD(2, 2), &wsd);
#endif

	/* initialize data structures */
	memset(&pfd, 0, sizeof(pfd));
	memset(users, 0, sizeof(users));
	for (i = 0; i != MAX_USERS; ++i)
		users[i].sock = -1;

	/* get listening port */
	listen_port = port;

	/* get options */
	telnet_options_t = options_t;
	telnet_options_len = options_len;

	/* create listening socket */
	if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return 1;
	}

	/* reuse address option */
	rs = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&rs, sizeof(rs));

	/* bind to listening addr/port */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(listen_port);
	if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		fprintf(stderr, "bind() failed: %s\n", strerror(errno));
		close(listen_sock);
		return 1;
	}

	/* listen for clients */
	if (listen(listen_sock, 5) == -1) {
		fprintf(stderr, "listen() failed: %s\n", strerror(errno));
		close(listen_sock);
		return 1;
	}

	printf("LISTENING ON PORT %d\n", listen_port);

	/* initialize listening descriptors */
	pfd[MAX_USERS].fd = listen_sock;
	pfd[MAX_USERS].events = POLLIN;

	/* loop for ever */
	for (;;) {
		/* prepare for poll */
		for (i = 0; i != MAX_USERS; ++i) {
			if (users[i].sock != -1) {
				pfd[i].fd = users[i].sock;
				pfd[i].events = POLLIN;
			} else {
				pfd[i].fd = -1;
				pfd[i].events = 0;
			}
		}

		/* poll */
		rs = poll(pfd, MAX_USERS + 1, -1);
		if (rs == -1 && errno != EINTR) {
			fprintf(stderr, "poll() failed: %s\n", strerror(errno));
			close(listen_sock);
			return 1;
		}

		/* new connection */
		if (pfd[MAX_USERS].revents & (POLLIN | POLLERR | POLLHUP)) {
			/* acept the sock */
			addrlen = sizeof(addr);
			if ((client_sock = accept(listen_sock, (struct sockaddr *)&addr,
					&addrlen)) == -1) {
				fprintf(stderr, "accept() failed: %s\n", strerror(errno));
				return 1;
			}

			printf("Connection received.\n");

			/* find a free user */
			for (i = 0; i != MAX_USERS; ++i)
				if (users[i].sock == -1)
					break;
			if (i == MAX_USERS) {
				printf("  rejected (too many users)\n");
				_send(client_sock, "Too many users.\r\n", 14);
				close(client_sock);
			}

			/* init, welcome */
			users[i].sock = client_sock;
			users[i].telnet = telnet_init(telopts, _event_handler, 0,
					&users[i]);

			telnet_printf(users[i].telnet, "\nWelcome, ABUP OTA's debug system!");
		}

		/* read from client */
		for (i = 0; i != MAX_USERS; ++i) {
			/* skip users that aren't actually connected */
			if (users[i].sock == -1)
				continue;

			if (pfd[i].revents & (POLLIN | POLLERR | POLLHUP)) {
				memset(buffer, 0, sizeof(buffer));
				if ((rs = recv(users[i].sock, buffer, sizeof(buffer), 0)) > 0) {
					telnet_recv(users[i].telnet, buffer, rs);
				} else if (rs == 0) {
					printf("Connection closed.\n");
					close(users[i].sock);
					users[i].sock = -1;
					if (users[i].name != 0) {
						_message(users[i].name, "** HAS DISCONNECTED **");
						free(users[i].name);
						users[i].name = 0;
					}
					telnet_free(users[i].telnet);
				} else if (errno != EINTR) {
					fprintf(stderr, "recv(client) failed: %s\n",
							strerror(errno));
					exit(1);
				}
			}
		}
	}

	/* not that we can reach this, but GCC will cry if it's not here */
	return 0;
}
