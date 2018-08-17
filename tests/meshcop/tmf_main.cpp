/*
 *    Copyright (c) 2018, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   The file provides a utility to extract net topology given ipv6 address of a thread node
 */

#include <stdio.h>
#include <stdlib.h>

#include "commissioner_proxy.hpp"
#include "tmf_client.hpp"

using namespace ot;
using namespace ot::BorderRouter;

int main(int argc, char *argv[])
{
    CommissionerProxy  p;
    TmfClient          client(&p);
    Json::StyledWriter writer;
    struct in6_addr    destAddr;

    if (argc != 3)
    {
        printf("Usage: tmf_client node_address save_file_name");
    }
    else
    {
        srand(time(0));
        inet_pton(AF_INET6, argv[1], &destAddr);
        {
            std::vector<struct in6_addr> addrs = client.QueryAllV6Addresses(destAddr);

            for (size_t i = 0; i < addrs.size(); i++)
            {
                char nameBuffer[100];
                inet_ntop(AF_INET6, &addrs[i], nameBuffer, sizeof(nameBuffer));
                printf("Addr %s\n", nameBuffer);
            }
        }

        {
            NetworkInfo networkInfo = client.TraverseNetwork(destAddr);

            std::string s  = writer.write(DumpNetworkInfoToJson(networkInfo));
            FILE *      fp = fopen(argv[2], "w");
            fprintf(fp, "%s", s.c_str());
            fclose(fp);
        }
    }
    return 0;
}
