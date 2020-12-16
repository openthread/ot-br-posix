/*
 *    Copyright (c) 2020, The OpenThread Authors.
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
 *   This file includes definition for Thread border router agent instance parameters.
 */

#ifndef OTBR_AGENT_INSATNCE_PARAMS_HPP_
#define OTBR_AGENT_INSATNCE_PARAMS_HPP_

namespace otbr {

/**
 * This class represents the agent instance parameters.
 *
 */
class InstanceParams
{
public:
    /**
     * This method gets the single `InstanceParams` instance.
     *
     * @returns  The single `InstanceParams` instance.
     *
     */
    static InstanceParams &Get(void);

    /**
     * This method sets the Thread network interface name.
     *
     * @param[in] aName  The Thread network interface name.
     *
     */
    void SetThreadIfName(const char *aName) { mThreadIfName = aName; }

    /**
     * This method gets the Thread network interface name.
     *
     * @returns The Thread network interface name.
     *
     */
    const char *GetThreadIfName(void) const { return mThreadIfName; }

    /**
     * This method sets the Backbone network interface name.
     *
     * @param[in] aName  The Backbone network interface name.
     *
     */
    void SetBackboneIfName(const char *aName) { mBackboneIfName = aName; }

    /**
     * This method gets the Backbone network interface name.
     *
     * @returns The Backbone network interface name.
     *
     */
    const char *GetBackboneIfName(void) const { return mBackboneIfName; }

private:
    InstanceParams()
        : mThreadIfName(nullptr)
        , mBackboneIfName(nullptr)
    {
    }

    const char *mThreadIfName;
    const char *mBackboneIfName;
};

} // namespace otbr

#endif // OTBR_AGENT_INSATNCE_PARAMS_HPP_
