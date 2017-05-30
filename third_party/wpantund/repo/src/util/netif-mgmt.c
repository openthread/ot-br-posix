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
#include "netif-mgmt.h"
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

#ifndef SIOCSIFLLADDR
#define SIOCSIFLLADDR SIOCSIFHWADDR
#endif

int
netif_mgmt_open(void)
{
	return socket(AF_INET6, SOCK_DGRAM, IPPROTO_IP);
}

void
netif_mgmt_close(int fd)
{
	close(fd);
}

int netif_mgmt_get_flags(int fd, const char* if_name)
{
	int ret = 0;
	int status = 0;
	struct ifreq ifr = { };

	strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

	status = ioctl(fd, SIOCGIFFLAGS, &ifr);

	require_string(status == 0, bail, strerror(errno));

	ret = ifr.ifr_flags;

bail:
	return ret;
}

int netif_mgmt_set_flags(int fd, const char* if_name, int flags)
{
	int ret = -1;
	struct ifreq ifr = { };

	strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

	ret = ioctl(fd, SIOCGIFFLAGS, &ifr);
	require_string(ret == 0, bail, strerror(errno));

	ifr.ifr_flags |= flags;

	ret = ioctl(fd, SIOCSIFFLAGS, &ifr);
	require_string(ret == 0, bail, strerror(errno));

bail:
	return ret;
}

int netif_mgmt_clear_flags(int fd, const char* if_name, int flags)
{
	int ret = -1;
	struct ifreq ifr = { };

	strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

	ret = ioctl(fd, SIOCGIFFLAGS, &ifr);
	require_string(ret == 0, bail, strerror(errno));

	ifr.ifr_flags &= ~flags;

	ret = ioctl(fd, SIOCSIFFLAGS, &ifr);
	require_string(ret == 0, bail, strerror(errno));

bail:
	return ret;
}

bool
netif_mgmt_is_up(int fd, const char* if_name)
{
	return ((netif_mgmt_get_flags(fd, if_name) & IFF_UP) == IFF_UP);
}

bool
netif_mgmt_is_running(int fd, const char* if_name)
{
	return ((netif_mgmt_get_flags(fd, if_name) & IFF_RUNNING) == IFF_RUNNING);
}

int
netif_mgmt_set_up(int fd, const char* if_name, bool value)
{
	if (value) {
		return netif_mgmt_set_flags(fd, if_name, IFF_UP);
	}

	return netif_mgmt_clear_flags(fd, if_name, IFF_UP|IFF_RUNNING);
}

int
netif_mgmt_set_running(int fd, const char* if_name, bool value)
{
	if (value) {
		return netif_mgmt_set_flags(fd, if_name, IFF_UP|IFF_RUNNING);
	}

	return netif_mgmt_clear_flags(fd, if_name, IFF_RUNNING);
}

int
netif_mgmt_set_mtu(int fd, const char* if_name, uint16_t mtu)
{
	int ret = -1;
	struct ifreq ifr = { };

	strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

	ifr.ifr_mtu = mtu;

	ret = ioctl(fd, SIOCSIFMTU, &ifr);

	require_string(ret == 0, bail, strerror(errno));

bail:
	return ret;
}

int
netif_mgmt_get_ifindex(int reqfd, const char* if_name) {
	int ret = -1;

#ifdef SIOGIFINDEX
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(struct ifreq));
	strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

	ret = ioctl(reqfd, SIOGIFINDEX, &ifr);

	if (ret >= 0) {
		ret = ifr.ifr_ifindex;
	}
#endif

	return ret;
}



static inline void
apply_mask(
	struct in6_addr *address, uint8_t mask
	)
{
	// Coverty might complain in this function, but don't believe it.
	// This code has been reviewed carefully and should not misbehave.

	if (mask > 128) {
		mask = 128;
	}

	memset(
		(void*)(address->s6_addr + ((mask + 7) / 8)),
		0,
		16 - ((mask + 7) / 8)
	);

	if (mask % 8) {
		address->s6_addr[mask / 8] &= ~(0xFF >> (mask % 8));
	}
}

int
netif_mgmt_add_ipv6_address(int reqfd, const char* if_name, const uint8_t addr[16], int prefixlen)
{
	int ret = -1;

#ifdef __APPLE__

	/************* Add address *************/

	struct in6_aliasreq addreq6 = { };

	strlcpy(addreq6.ifra_name, if_name, sizeof(addreq6.ifra_name));

	addreq6.ifra_addr.sin6_family = AF_INET6;
	addreq6.ifra_addr.sin6_len = sizeof(addreq6.ifra_addr);
	memcpy((void*)&addreq6.ifra_addr.sin6_addr, addr, 16);

	addreq6.ifra_prefixmask.sin6_family = AF_INET6;
	addreq6.ifra_prefixmask.sin6_len = sizeof(addreq6.ifra_prefixmask);
	memset((void*)&addreq6.ifra_prefixmask.sin6_addr, 0xFF, 16);
	apply_mask(&addreq6.ifra_prefixmask.sin6_addr, prefixlen);

	addreq6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	addreq6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
	addreq6.ifra_lifetime.ia6t_expire = ND6_INFINITE_LIFETIME;
	addreq6.ifra_lifetime.ia6t_preferred = ND6_INFINITE_LIFETIME;

	addreq6.ifra_flags |= IN6_IFF_NODAD;

	ret = ioctl(reqfd, SIOCAIFADDR_IN6, &addreq6);

	require_string(ret == 0 || errno == EALREADY, bail, strerror(errno));

#else

	/* Linux */

	// In linux, we need to remove the address first.
	netif_mgmt_remove_ipv6_address(reqfd, if_name, addr);

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

	struct in6_ifreq {
		struct in6_addr ifr6_addr;
		__u32 ifr6_prefixlen;
		unsigned int ifr6_ifindex;
	};

	struct sockaddr_in6 sai;
	int sockfd;
	struct in6_ifreq ifr6;

	memset(&sai, 0, sizeof(struct sockaddr));
	sai.sin6_family = AF_INET6;
	sai.sin6_port = 0;

	memcpy((void*)&sai.sin6_addr, addr, 16);

	memcpy((char*)&ifr6.ifr6_addr, (char*)&sai.sin6_addr,
		   sizeof(struct in6_addr));

	ret = netif_mgmt_get_ifindex(reqfd, if_name);

	require_string(ret >= 0, bail, strerror(errno));

	ifr6.ifr6_ifindex = ret;
	ifr6.ifr6_prefixlen = 64;

	ret = ioctl(reqfd, SIOCSIFADDR, &ifr6);

	require_string(ret == 0 || errno == EALREADY, bail, strerror(errno));

#endif
	ret = 0;
bail:

	return ret;
}

int
netif_mgmt_remove_ipv6_address(int reqfd, const char* if_name, const uint8_t addr[16])
{
	int ret = -1;

	struct sockaddr_in6 sai = { };

	memset(&sai, 0, sizeof(struct sockaddr));
	sai.sin6_family = AF_INET6;
	sai.sin6_port = 0;
	memcpy((void*)&sai.sin6_addr, addr, 16);

	/************* Remove address *************/

#ifdef __APPLE__
	sai.sin6_len = sizeof(sai);

	struct in6_ifreq ifreq6 = { };
	strlcpy(ifreq6.ifr_name, if_name, sizeof(ifreq6.ifr_name));

	ifreq6.ifr_addr = sai;

	ret = ioctl(reqfd, SIOCDIFADDR_IN6, &ifreq6);
#else
	struct in6_ifreq {
		struct in6_addr ifr6_addr;
		__u32 ifr6_prefixlen;
		unsigned int ifr6_ifindex;
	};

	struct in6_ifreq ifr6;

	ifr6.ifr6_addr = sai.sin6_addr;
	ifr6.ifr6_prefixlen = 64;

	ret = netif_mgmt_get_ifindex(reqfd, if_name);

	require_string(ret >= 0, bail, strerror(errno));

	ifr6.ifr6_ifindex = ret;

	ret = ioctl(reqfd, SIOCDIFADDR, &ifr6);
#endif

bail:
	return ret;
}

int
netif_mgmt_add_ipv6_route(int reqfd, const char* if_name, const uint8_t route[16], int prefixlen)
{
	int ret = -1;


#ifdef __APPLE__

	/************* Add ROUTE TODO *************/

#else
	/* Linux */

	struct ifreq ifr;
	struct in6_rtmsg rt;

	memset(&ifr, 0, sizeof(struct ifreq));

	strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

	memset(&rt, 0, sizeof(struct in6_rtmsg));
	memcpy(rt.rtmsg_dst.s6_addr, route, sizeof(struct in6_addr));
	rt.rtmsg_dst_len = prefixlen;
	rt.rtmsg_flags = RTF_UP;
	if (prefixlen == 128) {
		rt.rtmsg_flags |= RTF_HOST;
	}
	rt.rtmsg_metric = 512;

	ret = ioctl(reqfd, SIOGIFINDEX, &ifr);

	require_string(ret >= 0, bail, strerror(errno));

	rt.rtmsg_ifindex = ifr.ifr_ifindex;

	ret = ioctl(reqfd, SIOCADDRT, &rt);

	require_quiet(ret == 0 || errno == EALREADY || errno == EEXIST, bail);
#endif

	ret = 0;
bail:

	return ret;
}

int
netif_mgmt_remove_ipv6_route(int reqfd, const char* if_name, const uint8_t route[16], int prefixlen)
{
	int ret = -1;


#ifdef __APPLE__

	/************* Remove ROUTE TODO *************/

#else
	/* Linux */

	struct in6_rtmsg rt;

	memset(&rt, 0, sizeof(struct in6_rtmsg));

	memcpy(rt.rtmsg_dst.s6_addr, route, sizeof(struct in6_addr));

	rt.rtmsg_dst_len = prefixlen;
	rt.rtmsg_flags = RTF_UP;
	rt.rtmsg_metric = 512;

	if (prefixlen == 128) {
		rt.rtmsg_flags |= RTF_HOST;
	}

	ret = netif_mgmt_get_ifindex(reqfd, if_name);

	require_string(ret >= 0, bail, strerror(errno));

	rt.rtmsg_ifindex = ret;

	ret = ioctl(reqfd, SIOCDELRT, &rt);

	require_quiet(ret == 0 || errno == EALREADY || errno == EEXIST, bail);

#endif

	ret = 0;
bail:
	return ret;
}
