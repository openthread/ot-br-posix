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

/**
 * @file
 *   This file includes definitions of the vendor server. The implementation of the
 *   vendor server should be implemented by users.
 */
#ifndef OTBR_AGENT_VENDOR_HPP_
#define OTBR_AGENT_VENDOR_HPP_

#include "openthread-br/config.h"

#include "agent/application.hpp"

namespace otbr {

class Application;

namespace vendor {

/**
 * An interface for customized behavior depending on OpenThread API and state.
 */
class VendorServer
{
public:
    virtual ~VendorServer(void) = default;

    /**
     * Creates a new instance of VendorServer.
     *
     * Custom vendor servers should implement this method to return an object of the derived class.
     *
     * @param[in]  aApplication  The OTBR application.
     *
     * @returns  New derived VendorServer instance.
     */
    static std::shared_ptr<VendorServer> newInstance(Application &aApplication);

    /**
     * Initializes the vendor server.
     *
     * This will be called by `Application::Init()` after OpenThread instance and other built-in
     * servers have been created and initialized.
     */
    virtual void Init(void) = 0;
};

} // namespace vendor
} // namespace otbr
#endif // OTBR_AGENT_VENDOR_HPP_
