/*
 * Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sc_app.h"

#define SOCKET_PATH	INSTALLDIR"/.sc_app/socket"

int
main(int argc, char **argv)
{
	int Sock_FD;
	struct sockaddr_un Server;
	char OutBuffer[SYSCMD_MAX] = { 0 };
	char InBuffer[SOCKBUF_MAX];
	int Send_Length, Recv_Length;
	int White_Space;
	int Ret = 0;

	if ((Sock_FD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "ERROR: failed to get socket descriptor: %m\n");
		return -1;
	}

	Server.sun_family = AF_UNIX;
	(void) strcpy(Server.sun_path, SOCKET_PATH);
	Send_Length = sizeof(struct sockaddr_un);
	if (connect(Sock_FD, (struct sockaddr *)&Server, Send_Length) == -1) {
		fprintf(stderr, "ERROR: failed to connect to socket: %m\n");
		return -1;
	}

	for (int i = 0; (i < argc && strlen(OutBuffer) < SYSCMD_MAX); i++) {
		White_Space = 0;

		/* If argument has a whitespace, put a single-quote around it */
		if (strstr(argv[i], " ") != NULL) {
			White_Space = 1;
			(void) strncat(OutBuffer, "'",
				       (SYSCMD_MAX - strlen(OutBuffer)));
		}

		(void) strncat(OutBuffer, argv[i],
			       (SYSCMD_MAX - strlen(OutBuffer)));
		if (White_Space == 1) {
			(void) strncat(OutBuffer, "'",
				       (SYSCMD_MAX - strlen(OutBuffer)));
		}

		/* Don't add a whitespace after last argument */
		if ((i + 1) != argc) {
			(void) strncat(OutBuffer, " ",
				       (SYSCMD_MAX - strlen(OutBuffer)));
		}
	}

	if (send(Sock_FD, OutBuffer, strlen(OutBuffer), 0) == -1) {
		fprintf(stderr, "ERROR: failed to send command to sc_appd: %m\n");
		return -1;
	}

	while (1) {
		memset(InBuffer, 0, SOCKBUF_MAX);
		Recv_Length = recv(Sock_FD, InBuffer, SOCKBUF_MAX, 0);
		if (Recv_Length > 0) {
			if (strstr(InBuffer, "ERROR: ") != NULL) {
				fprintf(stderr, "%s", InBuffer);
				Ret = -1;
			} else {
				fprintf(stdout, "%s", InBuffer);
				Ret = 0;
			}

			continue;
		} else if (Recv_Length == -1) {
			fprintf(stderr, "ERROR: failed to receive output "
				"from sc_appd: %m\n");
			Ret = -1;
			break;
		} else {
			break;
		}
	}

	(void) close(Sock_FD);
	return Ret;
}
