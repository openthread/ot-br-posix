/*
 *  Copyright (c) 2021, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OTBR_UTILS_SOCKET_UTILS_HPP_
#define OTBR_UTILS_SOCKET_UTILS_HPP_

#include "openthread-br/config.h"

#include "common/code_utils.hpp"

#include <assert.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

enum SocketBlockOption
{
    kSocketBlock,    ///< The socket is blocking.
    kSocketNonBlock, ///< The socket is non-blocking.
};

/**
 * This function creates a socket with SOCK_CLOEXEC flag set.
 *
 * @param[in] aDomain       The communication domain.
 * @param[in] aType         The semantics of communication.
 * @param[in] aProtocol     The protocol to use.
 * @param[in] aBlockOption  Whether to add nonblock flags.
 *
 * @retval -1   Failed to create socket.
 * @retval ...  The file descriptor of the created socket.
 */
int SocketWithCloseExec(int aDomain, int aType, int aProtocol, SocketBlockOption aBlockOption);

/**
 * This function creates a Linux netlink NETLINK_ROUTE socket for receiving routing and link updates.
 *
 * @param[in]  aNlGroups  The netlink multicast groups to listen to.
 *
 * @retval  -1  Failed to create the netlink socket.
 * @retval ...  The file descriptor of the created netlink socket.
 */
int CreateNetLinkRouteSocket(uint32_t aNlGroups);

#endif // OTBR_UTILS_SOCKET_UTILS_HPP_
