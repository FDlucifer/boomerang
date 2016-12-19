/*
 * This file is part of net-yy-unix strace test.
 *
 * Copyright (c) 2013-2016 Dmitry V. Levin <ldv@altlinux.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tests.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

int
main(int ac, const char **av)
{
	assert(ac == 2);

	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	unsigned int sun_path_len = strlen(av[1]);
	assert(sun_path_len > 0 && sun_path_len <= sizeof(addr.sun_path));
	strncpy(addr.sun_path, av[1], sizeof(addr.sun_path));
	struct sockaddr * const listen_sa = tail_memdup(&addr, sizeof(addr));

	socklen_t * const len = tail_alloc(sizeof(socklen_t));
	*len = offsetof(struct sockaddr_un, sun_path) + strlen(av[1]) + 1;
	if (*len > sizeof(addr))
		*len = sizeof(addr);

	int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listen_fd < 0)
		perror_msg_and_skip("socket");
	unsigned long listen_inode = inode_of_sockfd(listen_fd);
	printf("socket(AF_UNIX, SOCK_STREAM, 0) = %d<UNIX:[%lu]>\n",
	       listen_fd, listen_inode);

	(void) unlink(av[1]);
	if (bind(listen_fd, listen_sa, *len))
		perror_msg_and_skip("bind");
	printf("bind(%d<UNIX:[%lu]>, {sa_family=AF_UNIX, sun_path=\"%s\"}"
	       ", %u) = 0\n", listen_fd, listen_inode, av[1], (unsigned) *len);

	if (listen(listen_fd, 1))
		perror_msg_and_skip("listen");
	printf("listen(%d<UNIX:[%lu,\"%s\"]>, 1) = 0\n",
	       listen_fd, listen_inode, av[1]);

	unsigned int * const optval = tail_alloc(sizeof(unsigned int));
	*len = sizeof(*optval);
	if (getsockopt(listen_fd, SOL_SOCKET, SO_PASSCRED, optval, len))
		perror_msg_and_fail("getsockopt");
	printf("getsockopt(%d<UNIX:[%lu,\"%s\"]>, SOL_SOCKET, SO_PASSCRED"
	       ", [%u], [%u]) = 0\n",
	       listen_fd, listen_inode, av[1], *optval, (unsigned) *len);

	memset(listen_sa, 0, sizeof(addr));
	*len = sizeof(addr);
	if (getsockname(listen_fd, listen_sa, len))
		perror_msg_and_fail("getsockname");
	printf("getsockname(%d<UNIX:[%lu,\"%s\"]>, {sa_family=AF_UNIX"
	       ", sun_path=\"%s\"}, [%u]) = 0\n", listen_fd, listen_inode,
	       av[1], av[1], (unsigned) *len);

	int connect_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (connect_fd < 0)
		perror_msg_and_fail("socket");
	unsigned long connect_inode = inode_of_sockfd(connect_fd);
	printf("socket(AF_UNIX, SOCK_STREAM, 0) = %d<UNIX:[%lu]>\n",
	       connect_fd, connect_inode);

	if (connect(connect_fd, listen_sa, *len))
		perror_msg_and_fail("connect");
	printf("connect(%d<UNIX:[%lu]>, {sa_family=AF_UNIX"
	       ", sun_path=\"%s\"}, %u) = 0\n",
	       connect_fd, connect_inode, av[1], (unsigned) *len);

	struct sockaddr * const accept_sa = tail_alloc(sizeof(addr));
	memset(accept_sa, 0, sizeof(addr));
	*len = sizeof(addr);
	int accept_fd = accept(listen_fd, accept_sa, len);
	if (accept_fd < 0)
		perror_msg_and_fail("accept");
	unsigned long accept_inode = inode_of_sockfd(accept_fd);
	printf("accept(%d<UNIX:[%lu,\"%s\"]>, {sa_family=AF_UNIX}"
	       ", [%u]) = %d<UNIX:[%lu->%lu,\"%s\"]>\n",
	       listen_fd, listen_inode, av[1], (unsigned) *len,
	       accept_fd, accept_inode, connect_inode, av[1]);

	memset(listen_sa, 0, sizeof(addr));
	*len = sizeof(addr);
	if (getpeername(connect_fd, listen_sa, len))
		perror_msg_and_fail("getpeername");
	printf("getpeername(%d<UNIX:[%lu->%lu]>, {sa_family=AF_UNIX"
	       ", sun_path=\"%s\"}, [%u]) = 0\n", connect_fd, connect_inode,
	       accept_inode, av[1], (unsigned) *len);

	char text[] = "text";
	assert(sendto(connect_fd, text, sizeof(text) - 1, MSG_DONTWAIT, NULL, 0)
	       == sizeof(text) - 1);
	printf("sendto(%d<UNIX:[%lu->%lu]>, \"%s\", %u, MSG_DONTWAIT"
	       ", NULL, 0) = %u\n",
	       connect_fd, connect_inode, accept_inode, text,
	       (unsigned) sizeof(text) - 1, (unsigned) sizeof(text) - 1);

	assert(recvfrom(accept_fd, text, sizeof(text) - 1, MSG_DONTWAIT, NULL, NULL)
	       == sizeof(text) - 1);
	printf("recvfrom(%d<UNIX:[%lu->%lu,\"%s\"]>, \"%s\", %u, MSG_DONTWAIT"
	       ", NULL, NULL) = %u\n",
	       accept_fd, accept_inode, connect_inode, av[1], text,
	       (unsigned) sizeof(text) - 1, (unsigned) sizeof(text) - 1);

	assert(close(connect_fd) == 0);
	printf("close(%d<UNIX:[%lu->%lu]>) = 0\n",
	       connect_fd, connect_inode, accept_inode);

	assert(close(accept_fd) == 0);
	printf("close(%d<UNIX:[%lu->%lu,\"%s\"]>) = 0\n",
	       accept_fd, accept_inode, connect_inode, av[1]);

	connect_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (connect_fd < 0)
		perror_msg_and_fail("socket");
	connect_inode = inode_of_sockfd(connect_fd);
	printf("socket(AF_UNIX, SOCK_STREAM, 0) = %d<UNIX:[%lu]>\n",
	       connect_fd, connect_inode);

	*optval = 1;
	*len = sizeof(*optval);
	if (setsockopt(connect_fd, SOL_SOCKET, SO_PASSCRED, optval, *len))
		perror_msg_and_fail("setsockopt");
	printf("setsockopt(%d<UNIX:[%lu]>, SOL_SOCKET, SO_PASSCRED"
	       ", [%u], %u) = 0\n",
	       connect_fd, connect_inode, *optval, (unsigned) *len);

	memset(listen_sa, 0, sizeof(addr));
	*len = sizeof(addr);
	if (getsockname(listen_fd, listen_sa, len))
		perror_msg_and_fail("getsockname");
	printf("getsockname(%d<UNIX:[%lu,\"%s\"]>, {sa_family=AF_UNIX"
	       ", sun_path=\"%s\"}, [%u]) = 0\n", listen_fd, listen_inode,
	       av[1], av[1], (unsigned) *len);

	if (connect(connect_fd, listen_sa, *len))
		perror_msg_and_fail("connect");
	printf("connect(%d<UNIX:[%lu]>, {sa_family=AF_UNIX"
	       ", sun_path=\"%s\"}, %u) = 0\n",
	       connect_fd, connect_inode, av[1], (unsigned) *len);

	memset(accept_sa, 0, sizeof(addr));
	*len = sizeof(addr);
	accept_fd = accept(listen_fd, accept_sa, len);
	if (accept_fd < 0)
		perror_msg_and_fail("accept");
	accept_inode = inode_of_sockfd(accept_fd);
	const char * const sun_path1 =
		((struct sockaddr_un *) accept_sa) -> sun_path + 1;
	printf("accept(%d<UNIX:[%lu,\"%s\"]>, {sa_family=AF_UNIX"
	       ", sun_path=@\"%s\"}, [%u]) = %d<UNIX:[%lu->%lu,\"%s\"]>\n",
	       listen_fd, listen_inode, av[1], sun_path1, (unsigned) *len,
	       accept_fd, accept_inode, connect_inode, av[1]);

	memset(listen_sa, 0, sizeof(addr));
	*len = sizeof(addr);
	if (getpeername(connect_fd, listen_sa, len))
		perror_msg_and_fail("getpeername");
	printf("getpeername(%d<UNIX:[%lu->%lu,@\"%s\"]>, {sa_family=AF_UNIX"
	       ", sun_path=\"%s\"}, [%u]) = 0\n", connect_fd, connect_inode,
	       accept_inode, sun_path1, av[1], (unsigned) *len);

	assert(sendto(connect_fd, text, sizeof(text) - 1, MSG_DONTWAIT, NULL, 0)
	       == sizeof(text) - 1);
	printf("sendto(%d<UNIX:[%lu->%lu,@\"%s\"]>, \"%s\", %u, MSG_DONTWAIT"
	       ", NULL, 0) = %u\n",
	       connect_fd, connect_inode, accept_inode, sun_path1, text,
	       (unsigned) sizeof(text) - 1, (unsigned) sizeof(text) - 1);

	assert(recvfrom(accept_fd, text, sizeof(text) - 1, MSG_DONTWAIT, NULL, NULL)
	       == sizeof(text) - 1);
	printf("recvfrom(%d<UNIX:[%lu->%lu,\"%s\"]>, \"%s\", %u, MSG_DONTWAIT"
	       ", NULL, NULL) = %u\n",
	       accept_fd, accept_inode, connect_inode, av[1], text,
	       (unsigned) sizeof(text) - 1, (unsigned) sizeof(text) - 1);

	assert(close(connect_fd) == 0);
	printf("close(%d<UNIX:[%lu->%lu,@\"%s\"]>) = 0\n",
	       connect_fd, connect_inode, accept_inode, sun_path1);

	assert(close(accept_fd) == 0);
	printf("close(%d<UNIX:[%lu->%lu,\"%s\"]>) = 0\n",
	       accept_fd, accept_inode, connect_inode, av[1]);

	assert(unlink(av[1]) == 0);

	assert(close(listen_fd) == 0);
	printf("close(%d<UNIX:[%lu,\"%s\"]>) = 0\n",
	       listen_fd, listen_inode, av[1]);

	puts("+++ exited with 0 +++");
	return 0;
}
