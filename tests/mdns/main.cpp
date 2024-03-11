/*
 *    Copyright (c) 2017, The OpenThread Authors.
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

#define OTBR_LOG_TAG "TEST"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <netinet/in.h>
#include <signal.h>

#include <functional>
#include <vector>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "mdns/mdns.hpp"

using namespace otbr;
using namespace otbr::Mdns;

static Publisher *sPublisher = nullptr;

typedef std::function<void(void)> TestRunner;

int RunMainloop(void)
{
    int rval = 0;

    while (true)
    {
        MainloopContext mainloop;

        mainloop.mMaxFd   = -1;
        mainloop.mTimeout = {INT_MAX, INT_MAX};
        FD_ZERO(&mainloop.mReadFdSet);
        FD_ZERO(&mainloop.mWriteFdSet);
        FD_ZERO(&mainloop.mErrorFdSet);

        MainloopManager::GetInstance().Update(mainloop);
        rval = select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet,
                      (mainloop.mTimeout.tv_sec == INT_MAX ? nullptr : &mainloop.mTimeout));

        if (rval < 0)
        {
            perror("select");
            break;
        }

        MainloopManager::GetInstance().Process(mainloop);
    }

    return rval;
}

std::function<void(otbrError aError)> ErrorChecker(std::string aMessage)
{
    return [aMessage](otbrError aError) {
        if (aError == OTBR_ERROR_NONE)
        {
            otbrLogInfo("Got success callback: %s", aMessage.c_str());
        }
        else
        {
            otbrLogEmerg("Got error %d callback: %s", aError, aMessage.c_str());
            exit(-1);
        }
    };
}

void PublishSingleServiceWithCustomHost(void)
{
    uint8_t              xpanid[kSizeExtPanId]           = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t              extAddr[kSizeExtAddr]           = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t              hostAddr[OTBR_IP6_ADDRESS_SIZE] = {0};
    const char           hostName[]                      = "custom-host";
    std::vector<uint8_t> keyData                         = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    Publisher::TxtData   txtData;
    Publisher::TxtList   txtList{
        {"nn", "cool"},
        {"xp", xpanid, sizeof(xpanid)},
        {"tv", "1.1.1"},
        {"xa", extAddr, sizeof(extAddr)},
    };

    otbrLogInfo("PublishSingleServiceWithCustomHost");

    hostAddr[0]  = 0x20;
    hostAddr[1]  = 0x02;
    hostAddr[15] = 0x01;

    Publisher::EncodeTxtData(txtList, txtData);

    sPublisher->PublishKey(hostName, keyData, ErrorChecker("publish key for host"));
    sPublisher->PublishHost(hostName, {Ip6Address(hostAddr)}, ErrorChecker("publish the host"));
    sPublisher->PublishService(hostName, "SingleService", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData,
                               ErrorChecker("publish the service"));
    sPublisher->PublishKey("SingleService._meshcop._udp", keyData, ErrorChecker("publish key for service"));
}

void PublishSingleServiceWithKeyAfterwards(void)
{
    uint8_t            hostAddr[OTBR_IP6_ADDRESS_SIZE] = {0};
    const char         hostName[]                      = "custom-host";
    Publisher::TxtData txtData;

    otbrLogInfo("PublishSingleServiceWithKeyAfterwards");

    hostAddr[0]  = 0x20;
    hostAddr[1]  = 0x02;
    hostAddr[15] = 0x01;

    txtData.push_back(0);

    sPublisher->PublishHost(hostName, {Ip6Address(hostAddr)}, ErrorChecker("publish the host"));

    sPublisher->PublishService(
        hostName, "SingleService", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData, [](otbrError aError) {
            std::vector<uint8_t> keyData = {0x55, 0xaa, 0xbb, 0xcc, 0x77, 0x33};

            SuccessOrDie(aError, "publish the service");

            sPublisher->PublishKey("SingleService._meshcop._udp", keyData, ErrorChecker("publish key for service"));
        });
}

void PublishMultipleServicesWithCustomHost(void)
{
    uint8_t              xpanid[kSizeExtPanId]           = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t              extAddr[kSizeExtAddr]           = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t              hostAddr[OTBR_IP6_ADDRESS_SIZE] = {0};
    const char           hostName1[]                     = "custom-host-1";
    const char           hostName2[]                     = "custom-host-2";
    std::vector<uint8_t> keyData1                        = {0x10, 0x20, 0x03, 0x15};
    std::vector<uint8_t> keyData2                        = {0xCA, 0xFE, 0xBE, 0xEF};
    Publisher::TxtData   txtData;
    Publisher::TxtList   txtList{
        {"nn", "cool"},
        {"xp", xpanid, sizeof(xpanid)},
        {"tv", "1.1.1"},
        {"xa", extAddr, sizeof(extAddr)},
    };

    otbrLogInfo("PublishMultipleServicesWithCustomHost");

    hostAddr[0]  = 0x20;
    hostAddr[1]  = 0x02;
    hostAddr[15] = 0x01;

    Publisher::EncodeTxtData(txtList, txtData);

    // For host1 and its services we register keys first, then host/services

    sPublisher->PublishKey(hostName1, keyData1, ErrorChecker("publish key for host1"));
    sPublisher->PublishKey("MultipleService11._meshcop._udp", keyData1, ErrorChecker("publish key for service11"));
    sPublisher->PublishKey("MultipleService12._meshcop._udp", keyData1, ErrorChecker("publish key for service12"));

    sPublisher->PublishHost(hostName1, {Ip6Address(hostAddr)}, ErrorChecker("publish the host1"));
    sPublisher->PublishService(hostName1, "MultipleService11", "_meshcop._udp", Publisher::SubTypeList{}, 12345,
                               txtData, ErrorChecker("publish service11"));
    sPublisher->PublishService(hostName1, "MultipleService12", "_meshcop._udp", Publisher::SubTypeList{}, 12345,
                               txtData, ErrorChecker("publish service12"));

    // For host2 and its services we register host and services first, then keys.

    sPublisher->PublishHost(hostName2, {Ip6Address(hostAddr)}, ErrorChecker("publish host2"));
    sPublisher->PublishService(hostName2, "MultipleService21", "_meshcop._udp", Publisher::SubTypeList{}, 12345,
                               txtData, ErrorChecker("publish service21"));
    sPublisher->PublishService(hostName2, "MultipleService22", "_meshcop._udp", Publisher::SubTypeList{}, 12345,
                               txtData, ErrorChecker("publish service22"));

    sPublisher->PublishKey(hostName2, keyData2, ErrorChecker("publish key for host2"));
    sPublisher->PublishKey("MultipleService21._meshcop._udp", keyData2, ErrorChecker("publish key for service21"));
    sPublisher->PublishKey("MultipleService22._meshcop._udp", keyData2, ErrorChecker("publish key for service22"));
}

void PublishSingleService(void)
{
    uint8_t            xpanid[kSizeExtPanId] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t            extAddr[kSizeExtAddr] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    Publisher::TxtData txtData;
    Publisher::TxtList txtList{
        {"nn", "cool"},
        {"xp", xpanid, sizeof(xpanid)},
        {"tv", "1.1.1"},
        {"xa", extAddr, sizeof(extAddr)},
    };

    otbrLogInfo("PublishSingleService");

    Publisher::EncodeTxtData(txtList, txtData);

    sPublisher->PublishService("", "SingleService", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData,
                               ErrorChecker("publish service"));
}

void PublishSingleServiceWithEmptyName(void)
{
    uint8_t            xpanid[kSizeExtPanId] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t            extAddr[kSizeExtAddr] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    Publisher::TxtData txtData;
    Publisher::TxtList txtList{
        {"nn", "cool"},
        {"xp", xpanid, sizeof(xpanid)},
        {"tv", "1.1.1"},
        {"xa", extAddr, sizeof(extAddr)},
    };

    otbrLogInfo("PublishSingleServiceWithEmptyName");

    Publisher::EncodeTxtData(txtList, txtData);

    sPublisher->PublishService("", "", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData,
                               ErrorChecker("publish (empty)._meshcop._udp"));
}

void PublishMultipleServices(void)
{
    uint8_t            xpanid[kSizeExtPanId] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t            extAddr[kSizeExtAddr] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    Publisher::TxtData txtData;
    Publisher::TxtList txtList1{
        {"nn", "cool1"},
        {"xp", xpanid, sizeof(xpanid)},
        {"tv", "1.1.1"},
        {"xa", extAddr, sizeof(extAddr)},
    };
    Publisher::TxtList txtList2{
        {"nn", "cool2"},
        {"xp", xpanid, sizeof(xpanid)},
        {"tv", "1.1.1"},
        {"xa", extAddr, sizeof(extAddr)},
    };

    otbrLogInfo("PublishMultipleServices");

    Publisher::EncodeTxtData(txtList1, txtData);

    sPublisher->PublishService("", "MultipleService1", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData,
                               ErrorChecker("publish MultipleService1._meshcop._udp"));

    Publisher::EncodeTxtData(txtList2, txtData);

    sPublisher->PublishService("", "MultipleService2", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData,
                               ErrorChecker("publish MultipleService2._meshcop._udp"));
}

void PublishUpdateServices(void)
{
    uint8_t            xpanidOld[kSizeExtPanId] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t            xpanidNew[kSizeExtPanId] = {0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41};
    uint8_t            extAddr[kSizeExtAddr]    = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    Publisher::TxtData txtData;
    Publisher::TxtList txtList1{
        {"nn", "cool"},
        {"xp", xpanidOld, sizeof(xpanidOld)},
        {"tv", "1.1.1"},
        {"xa", extAddr, sizeof(extAddr)},
    };
    Publisher::TxtList txtList2{
        {"nn", "coolcool"},
        {"xp", xpanidNew, sizeof(xpanidNew)},
        {"tv", "1.1.1"},
        {"xa", extAddr, sizeof(extAddr)},
    };

    otbrLogInfo("PublishUpdateServices");

    Publisher::EncodeTxtData(txtList1, txtData);

    sPublisher->PublishService("", "UpdateService", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData,
                               [](otbrError aError) { otbrLogResult(aError, "UpdateService._meshcop._udp"); });

    Publisher::EncodeTxtData(txtList2, txtData);

    sPublisher->PublishService("", "UpdateService", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData,
                               ErrorChecker("publish UpdateService._meshcop._udp"));
}

void PublishServiceSubTypes(void)
{
    Publisher::TxtData     txtData;
    Publisher::SubTypeList subTypeList{"_subtype1", "_SUBTYPE2"};

    otbrLogInfo("PublishServiceSubTypes");

    txtData.push_back(0);

    subTypeList.back() = "_SUBTYPE3";

    sPublisher->PublishService("", "ServiceWithSubTypes", "_meshcop._udp", subTypeList, 12345, txtData,
                               ErrorChecker("publish ServiceWithSubTypes._meshcop._udp"));
}

void PublishKey(void)
{
    std::vector<uint8_t> keyData = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};

    otbrLogInfo("PublishKey");

    sPublisher->PublishKey("SingleService._meshcop._udp", keyData, ErrorChecker("publish key for service"));
}

void PublishKeyWithServiceRemoved(void)
{
    uint8_t            hostAddr[OTBR_IP6_ADDRESS_SIZE] = {0};
    const char         hostName[]                      = "custom-host";
    Publisher::TxtData txtData;

    otbrLogInfo("PublishKeyWithServiceRemoved");

    hostAddr[0]  = 0x20;
    hostAddr[1]  = 0x02;
    hostAddr[15] = 0x01;

    txtData.push_back(0);

    sPublisher->PublishHost(hostName, {Ip6Address(hostAddr)}, ErrorChecker("publish the host"));

    sPublisher->PublishService(
        hostName, "SingleService", "_meshcop._udp", Publisher::SubTypeList{}, 12345, txtData, [](otbrError aError) {
            std::vector<uint8_t> keyData = {0x55, 0xaa, 0xbb, 0xcc, 0x77, 0x33};

            SuccessOrDie(aError, "publish the service");

            sPublisher->PublishKey("SingleService._meshcop._udp", keyData, [](otbrError aError) {
                SuccessOrDie(aError, "publish key for service");

                sPublisher->UnpublishService("SingleService", "_meshcop._udp", ErrorChecker("unpublish service"));
            });
        });
}

otbrError Test(TestRunner aTestRunner)
{
    otbrError error = OTBR_ERROR_NONE;

    sPublisher = Publisher::Create([aTestRunner](Publisher::State aState) {
        if (aState == Publisher::State::kReady)
        {
            aTestRunner();
        }
    });
    SuccessOrExit(error = sPublisher->Start());
    RunMainloop();

exit:
    Publisher::Destroy(sPublisher);
    return error;
}

void RecoverSignal(int aSignal)
{
    if (aSignal == SIGUSR1)
    {
        signal(SIGUSR1, SIG_DFL);
    }
    else if (aSignal == SIGUSR2)
    {
        signal(SIGUSR2, SIG_DFL);
    }
}

otbrError TestStopService(void)
{
    otbrError ret = OTBR_ERROR_NONE;

    otbrLogInfo("TestStopService");

    sPublisher = Publisher::Create([](Publisher::State aState) {
        if (aState == Publisher::State::kReady)
        {
            PublishSingleService();
        }
    });
    SuccessOrExit(ret = sPublisher->Start());
    signal(SIGUSR1, RecoverSignal);
    signal(SIGUSR2, RecoverSignal);
    RunMainloop();
    sPublisher->Stop();
    RunMainloop();
    SuccessOrExit(ret = sPublisher->Start());
    RunMainloop();

exit:
    Publisher::Destroy(sPublisher);
    return ret;
}

otbrError CheckTxtDataEncoderDecoder(void)
{
    otbrError            error = OTBR_ERROR_NONE;
    Publisher::TxtList   txtList;
    Publisher::TxtList   parsedTxtList;
    std::vector<uint8_t> txtData;

    // Encode empty `TxtList`

    SuccessOrExit(error = Publisher::EncodeTxtData(txtList, txtData));
    VerifyOrExit(txtData.size() == 1, error = OTBR_ERROR_PARSE);
    VerifyOrExit(txtData[0] == 0, error = OTBR_ERROR_PARSE);

    SuccessOrExit(error = Publisher::DecodeTxtData(parsedTxtList, txtData.data(), txtData.size()));
    VerifyOrExit(parsedTxtList.size() == 0, error = OTBR_ERROR_PARSE);

    // TxtList with one bool attribute

    txtList.clear();
    txtList.emplace_back("b1");

    SuccessOrExit(error = Publisher::EncodeTxtData(txtList, txtData));
    SuccessOrExit(error = Publisher::DecodeTxtData(parsedTxtList, txtData.data(), txtData.size()));
    VerifyOrExit(parsedTxtList == txtList, error = OTBR_ERROR_PARSE);

    // TxtList with one one key/value

    txtList.clear();
    txtList.emplace_back("k1", "v1");

    SuccessOrExit(error = Publisher::EncodeTxtData(txtList, txtData));
    SuccessOrExit(error = Publisher::DecodeTxtData(parsedTxtList, txtData.data(), txtData.size()));
    VerifyOrExit(parsedTxtList == txtList, error = OTBR_ERROR_PARSE);

    // TxtList with multiple entries

    txtList.clear();
    txtList.emplace_back("k1", "v1");
    txtList.emplace_back("b1");
    txtList.emplace_back("b2");
    txtList.emplace_back("k2", "valu2");

    SuccessOrExit(error = Publisher::EncodeTxtData(txtList, txtData));
    SuccessOrExit(error = Publisher::DecodeTxtData(parsedTxtList, txtData.data(), txtData.size()));
    VerifyOrExit(parsedTxtList == txtList, error = OTBR_ERROR_PARSE);

exit:
    return error;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if (CheckTxtDataEncoderDecoder() != OTBR_ERROR_NONE)
    {
        return 1;
    }

    if (argc < 2)
    {
        return 1;
    }

    otbrLogInit("otbr-mdns", OTBR_LOG_DEBUG, true, false);
    // allow quitting elegantly
    signal(SIGTERM, RecoverSignal);
    switch (argv[1][0])
    {
    case 's':
        switch (argv[1][1])
        {
        case 'c':
            ret = Test(PublishSingleServiceWithCustomHost);
            break;
        case 'e':
            ret = Test(PublishSingleServiceWithEmptyName);
            break;
        case 'k':
            ret = Test(PublishSingleServiceWithKeyAfterwards);
            break;
        default:
            ret = Test(PublishSingleService);
            break;
        }
        break;

    case 'm':
        ret = argv[1][1] == 'c' ? Test(PublishMultipleServicesWithCustomHost) : Test(PublishMultipleServices);
        break;

    case 'u':
        ret = Test(PublishUpdateServices);
        break;

    case 't':
        ret = Test(PublishServiceSubTypes);
        break;

    case 'k':
        ret = TestStopService();
        break;

    case 'y':
        ret = Test(PublishKey);
        break;

    case 'z':
        ret = Test(PublishKeyWithServiceRemoved);
        break;

    default:
        ret = 1;
        break;
    }

    return ret;
}
