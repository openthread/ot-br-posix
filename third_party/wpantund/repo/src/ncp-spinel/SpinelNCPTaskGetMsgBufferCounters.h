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

#ifndef __wpantund__SpinelNCPTaskGetMsgBufferCounters__
#define __wpantund__SpinelNCPTaskGetMsgBufferCounters__

#include <list>
#include <string>
#include "ValueMap.h"
#include "SpinelNCPTask.h"
#include "SpinelNCPInstance.h"

using namespace nl;
using namespace nl::wpantund;

namespace nl {
namespace wpantund {

class SpinelNCPTaskGetMsgBufferCounters : public SpinelNCPTask
{
public:
	enum ResultFormat
	{
		kResultFormat_String,        // Returns the counter info as a std::string (long line with all counters)
		kResultFormat_StringArray,   // Returns the counter info as an array of std::string(s) (one per counter)
		kResultFormat_ValueMap,      // Returns the counter info as a ValueMap dictionary.
	};

	SpinelNCPTaskGetMsgBufferCounters(
		SpinelNCPInstance* instance,
		CallbackWithStatusArg1 cb,
		ResultFormat result_format = kResultFormat_ValueMap
	);
	virtual int vprocess_event(int event, va_list args);

private:

	struct MsgBufferCounters
	{
		uint16_t mTotalBuffers;           ///< The number of buffers in the pool.
		uint16_t mFreeBuffers;            ///< The number of free message buffers.
		uint16_t m6loSendMessages;        ///< The number of messages in the 6lo send queue.
		uint16_t m6loSendBuffers;         ///< The number of buffers in the 6lo send queue.
		uint16_t m6loReassemblyMessages;  ///< The number of messages in the 6LoWPAN reassembly queue.
		uint16_t m6loReassemblyBuffers;   ///< The number of buffers in the 6LoWPAN reassembly queue.
		uint16_t mIp6Messages;            ///< The number of messages in the IPv6 send queue.
		uint16_t mIp6Buffers;             ///< The number of buffers in the IPv6 send queue.
		uint16_t mMplMessages;            ///< The number of messages in the MPL send queue.
		uint16_t mMplBuffers;             ///< The number of buffers in the MPL send queue.
		uint16_t mMleMessages;            ///< The number of messages in the MLE send queue.
		uint16_t mMleBuffers;             ///< The number of buffers in the MLE send queue.
		uint16_t mArpMessages;            ///< The number of messages in the ARP send queue.
		uint16_t mArpBuffers;             ///< The number of buffers in the ARP send queue.
		uint16_t mCoapClientMessages;     ///< The number of messages in the CoAP client send queue.
		uint16_t mCoapClientBuffers;      ///< The number of buffers in the CoAP client send queue.

		std::string get_as_string(void) const;
		std::list<std::string> get_as_string_array(void) const;
		ValueMap get_as_valuemap(void) const;
	};

	ResultFormat mResultFormat;
};


}; // namespace wpantund
}; // namespace nl


#endif /* defined(__wpantund__SpinelNCPTaskGetMsgBufferCounters__) */
