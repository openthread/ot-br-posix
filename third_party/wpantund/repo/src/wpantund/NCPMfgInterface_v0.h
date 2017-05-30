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
 */

#ifndef wpantund_NCPMfgInterface_v0_h
#define wpantund_NCPMfgInterface_v0_h

#include "NCPControlInterface.h"

namespace nl {
namespace wpantund {

class NCPMfgInterface_v0 {
public:
	virtual void mfg_start(CallbackWithStatus cb) { }


	virtual void mfg_finish(CallbackWithStatusArg1 cb) = 0;
	virtual void mfg_begin_test(uint8_t type, CallbackWithStatus cb) = 0;
	virtual void mfg_end_test(uint8_t type, CallbackWithStatus cb) = 0;
	virtual void mfg_tx_packet(const Data& packet, int16_t repeat, CallbackWithStatus cb) = 0;

	virtual void mfg_clockmon(bool enable, uint32_t timerId, CallbackWithStatus cb) = 0;
	virtual void mfg_gpio_set(uint8_t port_pin, uint8_t config, uint8_t value, CallbackWithStatus cb) = 0;
	virtual void mfg_gpio_get(uint8_t port_pin, CallbackWithStatusArg1 cb) = 0;
	virtual void mfg_channelcal(uint8_t channel, uint32_t duration,  CallbackWithStatus cb) = 0;
	virtual void mfg_channelcal_get(uint8_t channel, CallbackWithStatusArg1 cb) = 0;

	boost::signals2::signal<void(Data, uint8_t, int8_t)> mOnMfgRXPacket;
};

}; // namespace wpantund
}; // namespace nl

#endif
