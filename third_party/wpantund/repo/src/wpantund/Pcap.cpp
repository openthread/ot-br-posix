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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"

#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "Pcap.h"

using namespace nl;
using namespace wpantund;


PcapPacket::PcapPacket()
	: mLen(sizeof(PcapFrameHeader)), mStatus(kWPANTUNDStatus_Ok)
{
	mHeader.mSeconds = 0;
	mHeader.mMicroSeconds = 0;
	mHeader.mRecordedPayloadSize = sizeof(PcapPpiHeader);
	mHeader.mActualPayloadSize = sizeof(PcapPpiHeader);
	mHeader.mPpiHeader.mVersion = 0;
	mHeader.mPpiHeader.mFlags = 0;
	mHeader.mPpiHeader.mSize = sizeof(PcapPpiHeader);
	mHeader.mPpiHeader.mDLT = 0;
}

wpantund_status_t
PcapPacket::get_status(void)const
{
	return mStatus;
}

const uint8_t*
PcapPacket::get_data_ptr(void)const
{
	return mData;
}

int
PcapPacket::get_data_len(void)const
{
	return mLen;
}

PcapPacket&
PcapPacket::set_timestamp(struct timeval* tv)
{
	if (tv == NULL) {
		struct timeval x;
		gettimeofday(&x, NULL);

		mHeader.mSeconds = static_cast<uint32_t>(x.tv_sec);
		mHeader.mMicroSeconds = x.tv_usec;

	} else {
		mHeader.mSeconds = static_cast<uint32_t>(tv->tv_sec);
		mHeader.mMicroSeconds = tv->tv_usec;
	}

	return *this;
}

PcapPacket&
PcapPacket::set_dlt(uint32_t i)
{
	mHeader.mPpiHeader.mDLT = i;
	return *this;
}

PcapPacket&
PcapPacket::append_ppi_field(uint16_t type, const uint8_t* field_ptr, int field_len)
{
	PcapPpiFieldHeader field_header;

	assert(mLen <= sizeof(mData));

	if (field_len < 0) {
		mStatus = kWPANTUNDStatus_InvalidArgument;

	} else if (mLen + field_len + sizeof(PcapPpiFieldHeader) > sizeof(mData)) {
		mStatus = kWPANTUNDStatus_InvalidArgument;

	} else {
		field_header.mType = type;
		field_header.mSize = field_len;
		memcpy(mData + mLen, &field_header, sizeof(PcapPpiFieldHeader));
		memcpy(mData + mLen + sizeof(PcapPpiFieldHeader), field_ptr, field_len);
		mLen += field_len + sizeof(PcapPpiFieldHeader);
		mHeader.mRecordedPayloadSize += field_len + sizeof(PcapPpiFieldHeader);
		mHeader.mPpiHeader.mSize += field_len + sizeof(PcapPpiFieldHeader);
	}

	mHeader.mActualPayloadSize += field_len + sizeof(PcapPpiFieldHeader);

	return *this;
}

PcapPacket&
PcapPacket::append_payload(const uint8_t* payload_ptr, int payload_len)
{
	assert(mLen <= sizeof(mData));

	if (payload_len < 0) {
		mStatus = kWPANTUNDStatus_InvalidArgument;

	} else if (mLen + payload_len > sizeof(mData)) {
		memcpy(mData + mLen, payload_ptr, sizeof(mData) - mLen);
		mLen = sizeof(mData);
		mHeader.mRecordedPayloadSize += sizeof(mData) - mLen;

	} else {
		memcpy(mData + mLen, payload_ptr, payload_len);
		mLen += payload_len;
		mHeader.mRecordedPayloadSize += payload_len;
	}

	mHeader.mActualPayloadSize += payload_len;

	return *this;
}


PcapManager::PcapManager()
{
}

PcapManager::~PcapManager()
{
}

bool
PcapManager::is_enabled(void)
{
	return !mFDSet.empty();
}

const std::set<int>&
PcapManager::get_fd_set(void)
{
	return mFDSet;
}

int
PcapManager::insert_fd(int fd)
{
	int ret = -1;
	int save_errno;
	int set = 1;
	PcapGlobalHeader header;

	// Prepare the PCAP header.
	header.mMagic = PCAP_MAGIC;
	header.mVerMaj = PCAP_VERSION_MAJOR;
	header.mVerMin = PCAP_VERSION_MINOR;
	header.mGMTOffset = 0;
	header.mAccuracy = 0;
	header.mSnapshotLengthField = PCAP_PACKET_MAX_SIZE;
	header.mDLT = PCAP_DLT_PPI;

#ifdef SO_NOSIGPIPE
	setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif

	// Send the PCAP header.
	ret = static_cast<int>(write(fd, &header, sizeof(header)));

	if (ret < 0) {
		save_errno = errno;
		syslog(LOG_ERR, "PcapManager::insert_fd: Call to send() on fd %d failed: %s (%d)", fd, strerror(errno), errno);
		goto bail;
	}

	mFDSet.insert(fd);

	ret = 0;

bail:
	if (ret < 0) {
		errno = save_errno;
	}

	return ret;
}

int
PcapManager::new_fd(void)
{
	int ret = -1;
	int save_errno;
	int fd[2] = { -1, -1 };

	ret = socketpair(PF_UNIX, SOCK_DGRAM, 0, fd);

	if (ret < 0) {
		save_errno = errno;
		syslog(LOG_ERR, "PcapManager::new_fd: Call to socketpair() failed: %s (%d)", strerror(errno), errno);
		goto bail;
	}

	ret = insert_fd(fd[1]);

	if (ret < 0) {
		save_errno = errno;
		goto bail;
	}

	ret = fd[0];

bail:

	if (ret < 0) {
		close(fd[0]);
		close(fd[1]);

		errno = save_errno;
	}

	return ret;
}

void
PcapManager::close_fd_set(const std::set<int>& x)
{
	if (&x == &mFDSet) {
		// Special case where we are asked to close everything.
		std::set<int> copy(x);

		close_fd_set(copy);

	} else if (!x.empty()) {
		std::set<int>::const_iterator iter;

		for ( iter  = x.begin()
			; iter != x.end()
			; ++iter
		) {
			const int fd = *iter;
			syslog(LOG_INFO, "PcapManager::close_fd_set: Closing FD %d", fd);
			close(fd);
			mFDSet.erase(fd);
		}
		syslog(LOG_INFO, "PcapManager: %d pcap streams remaining", static_cast<int>(mFDSet.size()));
	}
}

void
PcapManager::push_packet(const PcapPacket& packet)
{
	std::set<int>::const_iterator iter;
	std::set<int> remove_set;

	require_noerr(packet.get_status(), bail);

	for ( iter  = mFDSet.begin()
	    ; iter != mFDSet.end()
		; ++iter
	) {
		int ret;

		// Send the PCAP frame.
		ret = static_cast<int>(write(
			*iter,
			packet.get_data_ptr(),
			packet.get_data_len()
		));

		__ASSERT_MACROS_check(ret >= 0);

		if (ret < 0) {
			// Since we can't remove this file descriptor
			// from the set while we are iterating through it,
			// we add it to the remove set for later removal.
			remove_set.insert(*iter);
		}
	}

	close_fd_set(remove_set);

bail:
	return;
}

int
PcapManager::update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout)
{
	std::set<int>::const_iterator iter;

	for ( iter  = mFDSet.begin()
	    ; iter != mFDSet.end()
		; ++iter
	) {
		const int fd = *iter;

		if (read_fd_set) {
			FD_SET(fd, read_fd_set);
		}

		if (error_fd_set) {
			FD_SET(fd, error_fd_set);
		}

		if (max_fd && (*max_fd < fd)) {
			*max_fd = fd;
		}
	}

	return 0;
}

void
PcapManager::process(void)
{
	if (is_enabled()) {
		fd_set fds;
		int max_fd(-1);
		int fds_ready;
		struct timeval timeout = {};

		FD_ZERO(&fds);

		update_fd_set(&fds, NULL, &fds, &max_fd, NULL);

		fds_ready = select(
			max_fd + 1,
			&fds,
			NULL,
			&fds,
			&timeout
		);

		if (fds_ready > 0) {
			// Tear down bad file descriptors.
			std::set<int> remove_set;
			std::set<int>::const_iterator iter;

			for ( iter  = mFDSet.begin()
				; (fds_ready > 0) && (iter != mFDSet.end())
				; ++iter
			) {
				int fd = *iter;
				if (!FD_ISSET(fd, &fds)) {
					continue;
				}
				fds_ready--;
				remove_set.insert(fd);
			}

			close_fd_set(remove_set);
		}
	}
}
