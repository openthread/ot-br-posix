/*
 *    Copyright (c) 2021, The OpenThread Authors.
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

#include "common/dns_utils.hpp"
#include "common/code_utils.hpp"

DnsNameType GetDnsNameType(const std::string &aFullName)
{
    size_t      transportPos = aFullName.rfind("._udp.");
    size_t      dotPos;
    DnsNameType nameType = kDnsNameTypeUnknown;

    if (transportPos == std::string::npos)
    {
        transportPos = aFullName.rfind("._tcp.");
    }

    VerifyOrExit(transportPos != std::string::npos, nameType = kDnsNameTypeHost);
    VerifyOrExit(transportPos > 0);

    dotPos = aFullName.find_last_of('.', transportPos - 1);
    VerifyOrExit(dotPos != std::string::npos, nameType = kDnsNameTypeService);
    VerifyOrExit(dotPos > 0);

    nameType = kDnsNameTypeInstance;

exit:
    return nameType;
}

otbrError SplitFullServiceInstanceName(const std::string &aFullName,
                                       std::string &      aInstanceName,
                                       std::string &      aType,
                                       std::string &      aDomain)
{
    otbrError error = OTBR_ERROR_INVALID_ARGS;
    size_t    dotPos[3];

    dotPos[0] = aFullName.find_first_of('.');
    VerifyOrExit(dotPos[0] != std::string::npos);

    dotPos[1] = aFullName.find_first_of('.', dotPos[0] + 1);
    VerifyOrExit(dotPos[1] != std::string::npos);

    dotPos[2] = aFullName.find_first_of('.', dotPos[1] + 1);
    VerifyOrExit(dotPos[2] != std::string::npos);

    error         = OTBR_ERROR_NONE;
    aInstanceName = aFullName.substr(0, dotPos[0]);
    aType         = aFullName.substr(dotPos[0] + 1, dotPos[2] - dotPos[0] - 1);
    aDomain       = aFullName.substr(dotPos[2] + 1, aFullName.size() - dotPos[2] - 1);

exit:
    return error;
}

otbrError SplitFullServiceName(const std::string &aFullName, std::string &aType, std::string &aDomain)
{
    otbrError error = OTBR_ERROR_INVALID_ARGS;
    size_t    dotPos[2];

    dotPos[0] = aFullName.find_first_of('.');
    VerifyOrExit(dotPos[0] != std::string::npos);

    dotPos[1] = aFullName.find_first_of('.', dotPos[0] + 1);
    VerifyOrExit(dotPos[1] != std::string::npos);

    error   = OTBR_ERROR_NONE;
    aType   = aFullName.substr(0, dotPos[1]);
    aDomain = aFullName.substr(dotPos[1] + 1);

exit:
    return error;
}

otbrError SplitFullHostName(const std::string &aFullName, std::string &aHostName, std::string &aDomain)
{
    otbrError error = OTBR_ERROR_INVALID_ARGS;
    size_t    dotPos;

    dotPos = aFullName.find_first_of('.');
    VerifyOrExit(dotPos != std::string::npos);

    error     = OTBR_ERROR_NONE;
    aHostName = aFullName.substr(0, dotPos);
    aDomain   = aFullName.substr(dotPos + 1, aFullName.size() - dotPos - 1);

exit:
    return error;
}
