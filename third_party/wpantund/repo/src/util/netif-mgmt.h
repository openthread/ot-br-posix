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
 *		This file implements the code which manages a network interface.
 *
 */

#ifndef wpantund_netif_mgmt_h
#define wpantund_netif_mgmt_h

#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS
extern int netif_mgmt_open();
extern void netif_mgmt_close(int fd);

extern int netif_mgmt_get_ifindex(int fd, const char* if_name);

extern int netif_mgmt_set_mtu(int fd, const char* if_name, uint16_t mtu);

extern int netif_mgmt_get_flags(int fd, const char* if_name);
extern int netif_mgmt_set_flags(int fd, const char* if_name, int flags);
extern int netif_mgmt_clear_flags(int fd, const char* if_name, int flags);

extern bool netif_mgmt_is_up(int fd, const char* if_name);
extern int netif_mgmt_set_up(int fd, const char* if_name, bool value);

extern bool netif_mgmt_is_running(int fd, const char* if_name);
extern int netif_mgmt_set_running(int fd, const char* if_name, bool value);

extern int netif_mgmt_add_ipv6_address(int fd, const char* if_name, const uint8_t addr[16], int prefixlen);
extern int netif_mgmt_remove_ipv6_address(int fd, const char* if_name, const uint8_t addr[16]);

extern int netif_mgmt_add_ipv6_route(int fd, const char* if_name, const uint8_t route[16], int prefixlen);
extern int netif_mgmt_remove_ipv6_route(int fd, const char* if_name, const uint8_t route[16], int prefixlen);
__END_DECLS


#endif
