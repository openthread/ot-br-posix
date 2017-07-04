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

#ifndef __wpantund__SpinelNCPInstance__
#define __wpantund__SpinelNCPInstance__

#include "NCPInstanceBase.h"
#include "SpinelNCPControlInterface.h"
#include "nlpt.h"
#include "SocketWrapper.h"
#include "SocketAsyncOp.h"

#include <queue>
#include <set>
#include <map>
#include <errno.h>
#include "spinel.h"

WPANTUND_DECLARE_NCPINSTANCE_PLUGIN(spinel, SpinelNCPInstance);

#define EVENT_NCP_MARKER         0xAB000000
#define EVENT_NCP(x)             ((x)|EVENT_NCP_MARKER)
#define IS_EVENT_FROM_NCP(x)     (((x)&~0xFFFFFF) == EVENT_NCP_MARKER)


#define EVENT_NCP_RESET                (0xFF0000|EVENT_NCP_MARKER)
#define EVENT_NCP_PROP_VALUE_IS        (0xFF0001|EVENT_NCP_MARKER)
#define EVENT_NCP_PROP_VALUE_INSERTED  (0xFF0002|EVENT_NCP_MARKER)
#define EVENT_NCP_PROP_VALUE_REMOVED   (0xFF0003|EVENT_NCP_MARKER)

#define NCP_FRAMING_OVERHEAD 3

#define CONTROL_REQUIRE_EMPTY_OUTBOUND_BUFFER_WITHIN(seconds, error_label) do { \
		EH_WAIT_UNTIL_WITH_TIMEOUT(seconds, (GetInstance(this)->mOutboundBufferLen <= 0) && GetInstance(this)->mOutboundCallback.empty()); \
		require_string(!eh_did_timeout, error_label, "Timed out while waiting " # seconds " seconds for empty outbound buffer"); \
	} while (0)

#define CONTROL_REQUIRE_OUTBOUND_BUFFER_FLUSHED_WITHIN(seconds, error_label) do { \
		static const int ___crsw_send_finished = 0xFF000000 | __LINE__; \
		static const int ___crsw_send_failed = 0xFE000000 | __LINE__; \
		__ASSERT_MACROS_check(GetInstance(this)->mOutboundCallback.empty()); \
		require(GetInstance(this)->mOutboundBufferLen > 0, error_label); \
		GetInstance(this)->mOutboundCallback = CALLBACK_FUNC_SPLIT( \
			boost::bind(&NCPInstanceBase::process_event_helper, GetInstance(this), ___crsw_send_finished), \
			boost::bind(&NCPInstanceBase::process_event_helper, GetInstance(this), ___crsw_send_failed) \
		); \
		GetInstance(this)->mOutboundBuffer[0] = mLastHeader; \
		EH_WAIT_UNTIL_WITH_TIMEOUT(seconds, (event == ___crsw_send_finished) || (event == ___crsw_send_failed)); \
		require_string(!eh_did_timeout, error_label, "Timed out while trying to send command"); \
		require_string(event == ___crsw_send_finished, error_label, "Failure while trying to send command"); \
	} while (0)

#define CONTROL_REQUIRE_PREP_TO_SEND_COMMAND_WITHIN(timeout, error_label) do { \
		CONTROL_REQUIRE_EMPTY_OUTBOUND_BUFFER_WITHIN(timeout, error_label); \
		GetInstance(this)->mLastTID = SPINEL_GET_NEXT_TID(GetInstance(this)->mLastTID); \
		mLastHeader = (SPINEL_HEADER_FLAG | SPINEL_HEADER_IID_0 | (GetInstance(this)->mLastTID << SPINEL_HEADER_TID_SHIFT)); \
	} while (false)

#define CONTROL_REQUIRE_COMMAND_RESPONSE_WITHIN(timeout, error_label) do { \
		EH_REQUIRE_WITHIN(	\
			timeout,	\
			IS_EVENT_FROM_NCP(event) && GetInstance(this)->mInboundHeader == mLastHeader, \
			error_label	\
		);	\
	} while (false)

namespace nl {
namespace wpantund {

class SpinelNCPTask;
class SpinelNCPControlInterface;

class SpinelNCPInstance : public NCPInstanceBase {
	friend class SpinelNCPControlInterface;
	friend class SpinelNCPTask;
	friend class SpinelNCPTaskDeepSleep;
	friend class SpinelNCPTaskWake;
	friend class SpinelNCPTaskJoin;
	friend class SpinelNCPTaskForm;
	friend class SpinelNCPTaskScan;
	friend class SpinelNCPTaskLeave;
	friend class SpinelNCPTaskSendCommand;
	friend class SpinelNCPTaskGetNetworkTopology;
	friend class SpinelNCPTaskGetMsgBufferCounters;

public:

	enum DriverState {
		INITIALIZING,
		INITIALIZING_WAITING_FOR_RESET,
		NORMAL_OPERATION
	};

public:
	SpinelNCPInstance(const Settings& settings = Settings());

	virtual ~SpinelNCPInstance();

	virtual SpinelNCPControlInterface& get_control_interface();

	virtual int vprocess_event(int event, va_list args);


protected:
	virtual char ncp_to_driver_pump();
	virtual char driver_to_ncp_pump();

	void start_new_task(const boost::shared_ptr<SpinelNCPTask> &task);

	virtual bool is_busy(void);

protected:

	int vprocess_init(int event, va_list args);
	int vprocess_disabled(int event, va_list args);
	int vprocess_associated(int event, va_list args);
	int vprocess_resume(int event, va_list args);
	int vprocess_offline(int event, va_list args);

	void handle_ncp_spinel_callback(unsigned int command, const uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len);
	void handle_ncp_spinel_value_is(spinel_prop_key_t key, const uint8_t* value_data_ptr, spinel_size_t value_data_len);
	void handle_ncp_spinel_value_inserted(spinel_prop_key_t key, const uint8_t* value_data_ptr, spinel_size_t value_data_len);
	void handle_ncp_spinel_value_removed(spinel_prop_key_t key, const uint8_t* value_data_ptr, spinel_size_t value_data_len);
	void handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state);

	virtual void address_was_added(const struct in6_addr& addr, int prefix_len);
	virtual void address_was_removed(const struct in6_addr& addr, int prefix_len);

	void check_operation_status(std::string operation, int status);

	uint32_t get_default_channel_mask(void);

private:

	void refresh_on_mesh_prefix(struct in6_addr *addr, uint8_t prefix_len, bool stable, uint8_t flags, bool isLocal);

public:
	static bool setup_property_supported_by_class(const std::string& prop_name);

	virtual std::set<std::string> get_supported_property_keys()const;

	virtual void property_get_value(const std::string& key, CallbackWithStatusArg1 cb);
	virtual void property_set_value(const std::string& key, const boost::any& value, CallbackWithStatus cb);
	virtual void property_insert_value(const std::string& key, const boost::any& value, CallbackWithStatus cb);
	virtual void property_remove_value(const std::string& key, const boost::any& value, CallbackWithStatus cb);

	virtual cms_t get_ms_to_next_event(void);

	virtual void reset_tasks(wpantund_status_t status = kWPANTUNDStatus_Canceled);

	static void handle_ncp_log(const uint8_t* data_ptr, int data_len);

	virtual void process(void);

private:
	struct SettingsEntry
	{
	public:
		SettingsEntry(const Data &command = Data(), unsigned int capability = 0) :
			mSpinelCommand(command),
			mCapability(capability)
		{
		}

		Data mSpinelCommand;
		unsigned int mCapability;
	};

	/* Map from property key to setting entry
	 *
	 * The map contains all parameters/properties that are retained and
	 * restored when NCP gets initialized.
	 *
	 * `Setting entry` contains an optional capability value and an associated
	 * spinel command.
	 *
	 * If the `capability` is present in the list of NCP capabilities , then
	 * the associated spinel command is sent to NCP after initialization.
	 */
	typedef std::map<std::string, SettingsEntry> SettingsMap;

private:
	SpinelNCPControlInterface mControlInterface;

	uint8_t mLastTID;

	uint8_t mLastHeader;

	uint8_t mInboundFrame[SPINEL_FRAME_MAX_SIZE];
	uint8_t mInboundHeader;
	spinel_size_t mInboundFrameSize;
	uint8_t mInboundFrameDataType;
	const uint8_t* mInboundFrameDataPtr;
	spinel_size_t mInboundFrameDataLen;
	uint16_t mInboundFrameHDLCCRC;

	uint8_t mOutboundBufferHeader[3];
	uint8_t mOutboundBuffer[SPINEL_FRAME_MAX_SIZE];
	uint8_t mOutboundBufferType;
	spinel_ssize_t mOutboundBufferLen;
	spinel_ssize_t mOutboundBufferSent;
	uint8_t mOutboundBufferEscaped[SPINEL_FRAME_MAX_SIZE*2];
	spinel_ssize_t mOutboundBufferEscapedLen;
	boost::function<void(int)> mOutboundCallback;

	int mTXPower;

	std::set<unsigned int> mCapabilities;
	uint32_t mDefaultChannelMask;

	bool mSetSteeringDataWhenJoinable;
	uint8_t mSteeringDataAddress[8];

	SettingsMap mSettings;
	SettingsMap::iterator mSettingsIter;

	DriverState mDriverState;

	// Protothreads and related state
	PT mSleepPT;
	PT mSubPT;

	int mSubPTIndex;

	Data mNetworkPSKc;
	Data mNetworkKey;
	uint32_t mNetworkKeyIndex;
	bool mXPANIDWasExplicitlySet;

	bool mResetIsExpected;

	bool mIsPcapInProgress;

	// Task management
	std::list<boost::shared_ptr<SpinelNCPTask> > mTaskQueue;

}; // class SpinelNCPInstance

extern class SpinelNCPInstance* gNCPInstance;

template<class C>
inline SpinelNCPInstance* GetInstance(C *x)
{
	return x->mInstance;
}

template<>
inline SpinelNCPInstance* GetInstance<SpinelNCPInstance>(SpinelNCPInstance *x)
{
	return x;
}

template<class C>
inline nl::wpantund::SpinelNCPControlInterface* GetInterface(C *x)
{
	return x->mInterface;
}

template<>
inline nl::wpantund::SpinelNCPControlInterface* GetInterface<nl::wpantund::SpinelNCPControlInterface>(nl::wpantund::SpinelNCPControlInterface *x)
{
	return x;
}

bool ncp_event_matches_header_from_args(int event, va_list args, uint8_t last_header);

int peek_ncp_callback_status(int event, va_list args);

int spinel_status_to_wpantund_status(int spinel_status);

}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__SpinelNCPInstance__) */
