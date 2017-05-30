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

#ifndef __wpantund__Pcap__
#define __wpantund__Pcap__

#include <set>
#include "wpan-error.h"
#include "time-utils.h"

namespace nl {
namespace wpantund {

#define PCAP_PACKET_MAX_SIZE        512

#define PCAP_DLT_PPI                192
#define PCAP_DLT_IEEE802_15_4       195
#define PCAP_DLT_IEEE802_15_4_NOFCS 230

#define PCAP_MAGIC                  0xa1b2c3d4
#define PCAP_VERSION_MAJOR          2
#define PCAP_VERSION_MINOR          4

#define PCAP_PPI_VERSION            0

#define PCAP_PPI_TYPE_SPINEL        61616

/* Additional reading:
 *
 * * DLT list: http://www.tcpdump.org/linktypes.html
 * * Info on PPI: http://www.cacetech.com/documents/PPI%20Header%20format%201.0.7.pdf
 */

struct PcapGlobalHeader {
	uint32_t mMagic;
	uint16_t mVerMaj;
	uint16_t mVerMin;
	int32_t  mGMTOffset;
	uint32_t mAccuracy;
	uint32_t mSnapshotLengthField;
	uint32_t mDLT;
};

struct PcapPpiHeader {
	uint8_t mVersion;
	uint8_t mFlags;
	uint16_t mSize;
	uint32_t mDLT;
	uint8_t  mPayloadData[0];
};

struct PcapPpiFieldHeader {
	uint16_t mType;
	uint16_t mSize;
	uint8_t  mData[0];
};

struct PcapFrameHeader {
	uint32_t mSeconds;
	uint32_t mMicroSeconds;
	uint32_t mRecordedPayloadSize;
	uint32_t mActualPayloadSize;
	union {
		struct PcapPpiHeader mPpiHeader;
		uint8_t  mPayloadData[0];
	};
};

class PcapPacket
{
public:
	PcapPacket();

	wpantund_status_t get_status(void)const;

	const uint8_t* get_data_ptr(void)const;

	int get_data_len(void)const;

	PcapPacket& set_timestamp(struct timeval* tv = NULL);

	PcapPacket& set_dlt(uint32_t i);

	PcapPacket& append_ppi_field(uint16_t type, const uint8_t* field_ptr, int field_len);

	PcapPacket& append_payload(const uint8_t* payload_ptr, int payload_len);

	PcapPacket& finish(void);

private:
	union {
		uint8_t         mData[PCAP_PACKET_MAX_SIZE];
		PcapFrameHeader mHeader;
	};
	int               mLen;
	wpantund_status_t mStatus;
};

class PcapManager
{
public:
	PcapManager();
	~PcapManager();

	bool is_enabled(void);

	const std::set<int>& get_fd_set(void);

	int new_fd(void);

	int insert_fd(int fd);

	void push_packet(const PcapPacket& packet);

	void process(void);

	int update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout);

	void close_fd_set(const std::set<int>& x);

private:
	std::set<int> mFDSet;
};

}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__NetworkRetain__) */
