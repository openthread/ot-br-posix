/*
 *
 * Copyright (c) 2016 Nest Labs, Inc.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *    Description:
 *		This file implements the code which managed the TUN interface.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 1
#endif

#include "assert-macros.h"
#include "pt.h"

#include <stdio.h>
#include <stdlib.h>
#include "tunnel.h"
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef __APPLE__
#include <linux/if_tun.h>
#endif

#include <net/if.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <net/route.h> // AF_ROUTE things

#ifdef __APPLE__
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>   // ND6_INFINITE_LIFETIME
#include <net/if_dl.h>      // struct sockaddr_dl
#include <net/if_utun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/kern_control.h>
#include <sys/ioctl.h>
#include <sys/sys_domain.h>
#include <sys/kern_event.h>
#define IFEF_NOAUTOIPV6LL   0x2000  /* Interface IPv6 LinkLocal address not provided by kernel */
#endif

#ifndef TUNNEL_TUNTAP_DEVICE
#define TUNNEL_TUNTAP_DEVICE               "/dev/net/tun"
#endif

int
tunnel_open(const char* tun_name)
{
	int fd = -1;
	char *device = NULL;

	if ((tun_name == NULL) || (tun_name[0] == 0)) {
		tun_name = TUNNEL_DEFAULT_INTERFACE_NAME;
	}

	syslog(LOG_INFO, "Opening tun interface socket with name \"%s\"", tun_name);

#if defined(UTUN_CONTROL_NAME)
	int error = 0;
	fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	struct sockaddr_ctl addr;

	/* get/set the id */
	struct ctl_info info;
	memset(&info, 0, sizeof(info));
	strncpy(info.ctl_name, UTUN_CONTROL_NAME, strlen(UTUN_CONTROL_NAME));
	error = ioctl(fd, CTLIOCGINFO, &info);

	if (error) {
		syslog(LOG_ERR, "Failed to open utun interface: %s", strerror(errno));
		close(fd);
		fd = -1;
		goto bail;
	}

	addr.sc_id = info.ctl_id;
	addr.sc_len = sizeof(addr);
	addr.sc_family = AF_SYSTEM;
	addr.ss_sysaddr = AF_SYS_CONTROL;
	addr.sc_unit = 0;  /* allocate dynamically */

	if (strncmp(tun_name, "utun", 4) == 0)
		addr.sc_unit = (int)strtol(tun_name + 4, NULL, 10) + 1;

	error = connect(fd, (struct sockaddr*)&addr, sizeof(addr));

	if (error && errno == EBUSY) {
		addr.sc_unit = 0;  /* allocate dynamically */
		error = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
	}

	if (error) {
		syslog(LOG_ERR, "Failed to open tun interface: %s", strerror(errno));
		close(fd);
		fd = -1;
		goto bail;
	}

	goto bail;

#else

#ifdef __APPLE__
	if (strncmp(tun_name, "utun", 4) == 0)
		tun_name = "tun0";
	asprintf(&device, "/dev/%s", tun_name);
#else
	device = strdup(TUNNEL_TUNTAP_DEVICE);
#endif

	require(NULL != device, bail);

	fd = open(device, O_RDWR | O_NONBLOCK);

	if (0 > fd) {
		syslog(LOG_ERR, "Failed to open tun interface: %s", strerror(errno));
		perror("open-tun");
		goto bail;
	}
#endif

#ifdef TUNSETIFF
	struct ifreq ifr = { .ifr_flags = IFF_TUN | IFF_NO_PI };
	strncpy(ifr.ifr_name, tun_name, IFNAMSIZ);

	require(0 == ioctl(fd, TUNSETIFF, (void*)&ifr), bail);

	// Verify that the name was set. If it wasn't
	// we need to fail.
	char name[20] = "";

	if (tunnel_get_name(fd, name, sizeof(name)) != 0) {
		syslog(LOG_ERR, "Unable to set name on tun interface: %s", strerror(errno));
		perror("open-tun");
		close(fd);
		fd = -1;
		goto bail;
	}

	if (name[0] == 0) {
		syslog(LOG_ERR, "Unable to set name on tun interface");
		close(fd);
		fd = -1;
		goto bail;
	}

#endif

#if defined(TUNSETLINK) && defined(ARPHRD_6LOWPAN)
	int val = ARPHRD_6LOWPAN;

	if (ioctl(fd, TUNSETLINK, (unsigned long) val) < 0) {
		perror("TUNSETLINK");
	}
#endif

bail:
	free(device);
	return fd;
}

void
tunnel_close(int fd)
{
	close(fd);
}

int
tunnel_get_name(
    int fd, char* name, int maxlen
    )
{
	int ret = -1;

	if (maxlen && name) name[0] = 0;
#if defined(UTUN_CONTROL_NAME)
	socklen_t len = maxlen;
	if (0 == getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, name, &len)) {
		ret = 0;
		goto bail;
	}
#elif defined(TUNGETIFF)
	struct ifreq ifr = { };
	require(0 == ioctl(fd, TUNGETIFF, (void*)&ifr), bail);
	strncpy(name, ifr.ifr_name, maxlen);
#else
	struct stat st;
	ret = fstat(fd, &st);
	if (ret) {
		perror("tunnel_get_name: fstat failed.");
		goto bail;
	}
	devname_r(st.st_rdev, S_IFCHR, name, (int)maxlen);
#endif
	ret = 0;
bail:
	return ret;
}

int
tunnel_set_hwaddr(int fd, uint8_t *addr, int addr_len)
{
	// TODO: Implement me
	// Maybe also TUNSETLINK
	// Should use SIOCSIFHWADDR...?
	return -1;
}

