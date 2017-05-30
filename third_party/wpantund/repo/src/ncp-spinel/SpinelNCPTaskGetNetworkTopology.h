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

#ifndef __wpantund__SpinelNCPTaskGetNetworkTopology__
#define __wpantund__SpinelNCPTaskGetNetworkTopology__

#include <list>
#include <string>
#include "ValueMap.h"
#include "SpinelNCPTask.h"
#include "SpinelNCPInstance.h"

using namespace nl;
using namespace nl::wpantund;

namespace nl {
namespace wpantund {

class SpinelNCPTaskGetNetworkTopology : public SpinelNCPTask
{
public:

	enum Type
	{
		kChildTable,                   // Get the child table
		kNeighborTable                 // Get the neighbor table
	};

	enum ResultFormat
	{
		kResultFormat_StringArray,     // Returns the child/neighbor table as an array of std::string(s) (one per child).
		kResultFormat_ValueMapArray,   // Returns the child/neighbor table as an array of ValueMap dictionary.
	};

	enum
	{
		kThreadMode_RxOnWhenIdle        = (1 << 3),
		kThreadMode_SecureDataRequest   = (1 << 2),
		kThreadMode_FullFunctionDevice  = (1 << 1),
		kThreadMode_FullNetworkData     = (1 << 0),
	};

	// This struct defines a common table entry to store either a child info or a neighbor info
	struct TableEntry
	{
		Type      mType;     // Indicates if this entry is for a child or a neighbor

		// Common fields for both child info and neighbor info
		uint8_t   mExtAddress[8];
		uint32_t  mAge;
		uint16_t  mRloc16;
		uint8_t   mLinkQualityIn;
		int8_t    mAverageRssi;
		int8_t    mLastRssi;
		bool      mRxOnWhenIdle : 1;
		bool      mSecureDataRequest : 1;
		bool      mFullFunction : 1;
		bool      mFullNetworkData : 1;

		// Child info only
		uint32_t  mTimeout;
		uint8_t   mNetworkDataVersion;

		// Neighbor info only
		uint32_t  mLinkFrameCounter;
		uint32_t  mMleFrameCounter;
		bool      mIsChild : 1;

	public:
		std::string get_as_string(void) const;
		ValueMap get_as_valuemap(void) const;
	};

	typedef std::list<TableEntry> Table;

public:
	SpinelNCPTaskGetNetworkTopology(
		SpinelNCPInstance *instance,
		CallbackWithStatusArg1 cb,
		Type table_type = kChildTable,
		ResultFormat result_format = kResultFormat_StringArray
	);
	virtual int vprocess_event(int event, va_list args);

	// Parses the spinel child table property and updates the child_table
	static int prase_child_table(const uint8_t *data_in, spinel_size_t data_len, Table& child_table);

	// Parses the spinel neighbor table property and updates the neighbor_table
	static int prase_neighbor_table(const uint8_t *data_in, spinel_size_t data_len, Table& neighbor_table);

private:
	Type mType;
	Table mTable;
	ResultFormat mResultFormat;
};


}; // namespace wpantund
}; // namespace nl


#endif /* defined(__wpantund__SpinelNCPTaskGetNetworkTopology__) */
