/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#ifndef OTBR_REST_ACTIONS_HANDLER_HPP_
#define OTBR_REST_ACTIONS_HANDLER_HPP_

#include <memory>

#include "action.hpp"

namespace otbr {
namespace rest {
namespace actions {

class Handler
{
public:
    /**
     * Validate a json object for the action.
     */
    bool Validate(const cJSON &aJson) const { return mValidate(aJson); }

    /**
     * Create an action object from a json object.
     */
    std::unique_ptr<BasicActions> Create(const cJSON &aJson, Services &aServices) const
    {
        return mCreate(aJson, aServices);
    }

    /**
     * Make a handler for a specific action type.
     */
    template <typename ActionTy> static Handler MakeHandler(void)
    {
        return Handler{Helper<ActionTy>::Validate, Helper<ActionTy>::Build};
    }

private:
    Handler(bool (*aValidate)(const cJSON &aJson),
            std::unique_ptr<BasicActions> (*aCreate)(const cJSON &aJson, Services &aServices))
        : mValidate(aValidate)
        , mCreate(aCreate)
    {
    }

    bool (*mValidate)(const cJSON &aJson);
    std::unique_ptr<BasicActions> (*mCreate)(const cJSON &aJson, Services &aServices);

    template <typename ActionTy> struct Helper
    {
        static bool Validate(const cJSON &aJson) { return ActionTy::Validate(aJson); }

        static std::unique_ptr<BasicActions> Build(const cJSON &aJson, Services &aServices)
        {
            return std::unique_ptr<BasicActions>(new ActionTy(aJson, aServices));
        }
    };
};

const Handler *FindHandler(const char *aName);

} // namespace actions
} // namespace rest
} // namespace otbr

#endif // OTBR_REST_ACTIONS_HANDLER_HPP_
