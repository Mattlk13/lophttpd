/*
 * Copyright (C) 2014 Sebastian Krahmer.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Sebastian Krahmer.
 * 4. The name Sebastian Krahmer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <cstdio>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/disk.h>
#include "socket.h"
#include "flavor.h"
#include "lonely.h"


namespace flavor {

using namespace ns_socket;
using namespace std;


int accept(int fd, struct sockaddr *saddr, socklen_t *slen, int flags)
{
	int afd = 0;

	if ((afd = accept(fd, saddr, slen)) < 0)
		return -1;
	if (flags == NONBLOCK) {
		// no error check
		fcntl(afd, F_SETFL, O_RDWR|O_NONBLOCK);
	}
	return afd;
}


bool servable_device(const struct stat &st)
{
	return S_ISBLK(st.st_mode);
}


bool servable_file(const struct stat &st)
{
	// no S_ISLNK() since stat() was used
	return S_ISBLK(st.st_mode) || S_ISREG(st.st_mode) || S_ISDIR(st.st_mode);
}


int device_size(const std::string &path, off_t &size)
{
	int fd = ::open(path.c_str(), O_RDONLY|O_NOCTTY);
	if (fd < 0)
		return -1;

	int r = 0;
	if (ioctl(fd, DKIOCGETBLOCKCOUNT, &size) < 0)
		r = -1;
	close(fd);
	return r;
}


int in_send_queue(int fd)
{
	return 0;
}


ssize_t sendfile(int peer, int fd, off_t *offset, size_t n, off_t &left, off_t &copied, int ftype)
{
	if (n > MAX_SEND_SIZE)
		return -1;

	ssize_t r = 0, l = 0;

	// proc and sys files
	if (ftype == FILE_PROC) {
		char buf[MAX_SEND_SIZE], siz[32];
		r = pread(fd, buf, n, *offset);
		if (r < 0) {
			if (errno == EAGAIN)
				errno = EBADF;
			return -1;
		} else if (r > 0) {
			l = snprintf(siz, sizeof(siz), "%x\r\n", (int)r);
			if (writen(peer, siz, l) != l)
				return -1;
			if (writen(peer, buf, r) != r)
				return -1;
			if (writen(peer, "\r\n", 2) != 2)
				return -1;
			*offset += r;
			copied += r;
		} else {
			if (writen(peer, "0\r\n\r\n", 5) != 5)
				return -1;
			left = 0;
			r = 5;
		}
		return r;
	}

	off_t count = n;
	if (::sendfile(fd, peer, *offset, &count, NULL, 0) != 0)
		return -1;

	left -= count;
	copied += count;
	*offset += count;
	return r;
}


int sandbox()
{
	return 0;
}


} // namespace flavor

