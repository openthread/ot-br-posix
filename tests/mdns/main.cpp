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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

#include "agent/mdns.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"

using namespace ot::BorderRouter;

static struct Context
{
    Mdns::Publisher *mPublisher;
    bool             mUpdate;
} sContext;

int Mainloop(Mdns::Publisher &aPublisher)
{
    int rval = 0;

    while (true)
    {
        fd_set         readFdSet;
        fd_set         writeFdSet;
        fd_set         errorFdSet;
        int            maxFd   = -1;
        struct timeval timeout = {INT_MAX, INT_MAX};

        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);

        aPublisher.UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, (timeout.tv_sec == INT_MAX ? NULL : &timeout));

        if (rval < 0)
        {
            perror("select() failed");
            break;
        }

        aPublisher.Process(readFdSet, writeFdSet, errorFdSet);
    }

    return rval;
}

void PublishSingleService(void *aContext, Mdns::State aState)
{
    assert(aContext == &sContext);

    if (aState == Mdns::kStateReady)
    {
        assert(OTBR_ERROR_NONE == sContext.mPublisher->PublishService(12345, "SingleService", "_meshcop._udp.", "nn",
                                                                      "cool", "xp", "1122334455667788", NULL));
    }
}

void PublishMultipleServices(void *aContext, Mdns::State aState)
{
    assert(aContext == &sContext);

    if (aState == Mdns::kStateReady)
    {
        assert(OTBR_ERROR_NONE == sContext.mPublisher->PublishService(12345, "MultipleService1", "_meshcop._udp.", "nn",
                                                                      "cool1", "xp", "1122334455667788", NULL));
        assert(OTBR_ERROR_NONE == sContext.mPublisher->PublishService(12346, "MultipleService2", "_meshcop._udp.", "nn",
                                                                      "cool2", "xp", "1122334455667788", NULL));
    }
}

void PublishUpdateServices(void *aContext, Mdns::State aState)
{
    assert(aContext == &sContext);

    if (aState == Mdns::kStateReady)
    {
        if (!sContext.mUpdate)
        {
            assert(OTBR_ERROR_NONE == sContext.mPublisher->PublishService(12345, "UpdateService", "_meshcop._udp.",
                                                                          "nn", "cool", "xp", "1122334455667788",
                                                                          NULL));
        }
        else
        {
            assert(OTBR_ERROR_NONE == sContext.mPublisher->PublishService(12345, "UpdateService", "_meshcop._udp.",
                                                                          "nn", "coolcool", "xp", "8877665544332211",
                                                                          NULL));
        }
    }
}

otbrError TestSingleService(void)
{
    otbrError ret = OTBR_ERROR_NONE;

    Mdns::Publisher *pub = Mdns::Publisher::Create(AF_UNSPEC, NULL, NULL, PublishSingleService, &sContext);
    sContext.mPublisher  = pub;
    SuccessOrExit(ret = pub->Start());
    Mainloop(*pub);

exit:
    Mdns::Publisher::Destroy(pub);
    return ret;
}

otbrError TestMultipleServices(void)
{
    otbrError ret = OTBR_ERROR_NONE;

    Mdns::Publisher *pub = Mdns::Publisher::Create(AF_UNSPEC, NULL, NULL, PublishMultipleServices, &sContext);
    sContext.mPublisher  = pub;
    SuccessOrExit(ret = pub->Start());
    Mainloop(*pub);

exit:
    Mdns::Publisher::Destroy(pub);
    return ret;
}

otbrError TestUpdateService(void)
{
    otbrError ret = OTBR_ERROR_NONE;

    Mdns::Publisher *pub = Mdns::Publisher::Create(AF_UNSPEC, NULL, NULL, PublishUpdateServices, &sContext);
    sContext.mPublisher  = pub;
    sContext.mUpdate     = false;
    SuccessOrExit(ret = pub->Start());
    sContext.mUpdate = true;
    PublishUpdateServices(&sContext, Mdns::kStateReady);
    Mainloop(*pub);

exit:
    Mdns::Publisher::Destroy(pub);
    return ret;
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

    Mdns::Publisher *pub = Mdns::Publisher::Create(AF_UNSPEC, NULL, NULL, PublishSingleService, &sContext);
    sContext.mPublisher  = pub;
    SuccessOrExit(ret = pub->Start());
    signal(SIGUSR1, RecoverSignal);
    signal(SIGUSR2, RecoverSignal);
    Mainloop(*pub);
    sContext.mPublisher->Stop();
    Mainloop(*pub);
    SuccessOrExit(ret = sContext.mPublisher->Start());
    Mainloop(*pub);

exit:
    Mdns::Publisher::Destroy(pub);
    return ret;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if (argc < 2)
    {
        return 1;
    }

    otbrLogInit("otbr-mdns", OTBR_LOG_DEBUG, true);
    // allow quitting elegantly
    signal(SIGTERM, RecoverSignal);
    switch (argv[1][0])
    {
    case 's':
        ret = TestSingleService();
        break;

    case 'm':
        ret = TestMultipleServices();
        break;

    case 'u':
        ret = TestUpdateService();
        break;

    case 'k':
        ret = TestStopService();
        break;

    default:
        ret = 1;
        break;
    }

    return ret;
}
