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

#include "SpinelNCPInstance.h"
#include "time-utils.h"
#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "socket-utils.h"
#include <stdexcept>
#include <sys/file.h>
#include "SuperSocket.h"

using namespace nl;
using namespace wpantund;

#define HDLC_BYTE_FLAG             0x7E
#define HDLC_BYTE_ESC              0x7D
#define HDLC_BYTE_XON              0x11
#define HDLC_BYTE_XOFF             0x13
#define HDLC_BYTE_SPECIAL          0xF8
#define HDLC_ESCAPE_XFORM          0x20

static bool
hdlc_byte_needs_escape(uint8_t byte)
{
	switch(byte) {
	case HDLC_BYTE_SPECIAL:
	case HDLC_BYTE_ESC:
	case HDLC_BYTE_FLAG:
	case HDLC_BYTE_XOFF:
	case HDLC_BYTE_XON:
		return true;

	default:
		return false;
	}
}

static uint16_t
hdlc_crc16(uint16_t aFcs, uint8_t aByte)
{
#if 1
	// CRC-16/CCITT, CRC-16/CCITT-TRUE, CRC-CCITT
	// width=16 poly=0x1021 init=0x0000 refin=true refout=true xorout=0x0000 check=0x2189 name="KERMIT"
	// http://reveng.sourceforge.net/crc-catalogue/16.htm#crc.cat.kermit
    static const uint16_t sFcsTable[256] =
    {
        0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
        0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
        0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
        0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
        0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
        0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
        0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
        0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
        0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
        0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
        0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
        0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
        0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
        0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
        0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
        0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
        0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
        0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
        0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
        0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
        0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
        0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
        0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
        0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
        0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
        0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
        0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
        0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
        0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
        0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
        0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
        0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
    };
    return (aFcs >> 8) ^ sFcsTable[(aFcs ^ aByte) & 0xff];
#else
	// CRC-16/CCITT-FALSE, same CRC as 802.15.4
	// width=16 poly=0x1021 init=0xffff refin=false refout=false xorout=0x0000 check=0x29b1 name="CRC-16/CCITT-FALSE"
	// http://reveng.sourceforge.net/crc-catalogue/16.htm#crc.cat.crc-16-ccitt-false
	aFcs = (uint16_t)((aFcs >> 8) | (aFcs << 8));
	aFcs ^= aByte;
	aFcs ^= ((aFcs & 0xff) >> 4);
	aFcs ^= (aFcs << 12);
	aFcs ^= ((aFcs & 0xff) << 5);
	return aFcs;
#endif
}

char
SpinelNCPInstance::ncp_to_driver_pump()
{
	struct nlpt*const pt = &mNCPToDriverPumpPT;
//	unsigned int prop_key = 0;
	unsigned int command_value = 0;
	uint8_t byte;

	// Automatically detect socket resets and behave accordingly.
	if (mSerialAdapter->did_reset()) {
		syslog(LOG_NOTICE, "[-NCP-]: Socket Reset");
		NLPT_INIT(&mNCPToDriverPumpPT);
		NLPT_INIT(&mDriverToNCPPumpPT);

		process_event(EVENT_NCP_CONN_RESET);
	}

	NLPT_BEGIN(pt);

	// This macro abstracts the logic to read a single character into
	// `data`, in a protothreads-friendly way.
#define READ_CHARACTER(pt, data, on_fail) \
	while(1) {                                  \
		NLPT_WAIT_UNTIL_READABLE_OR_COND(pt, mSerialAdapter->get_read_fd(), mSerialAdapter->can_read()); \
		ssize_t retlen = mSerialAdapter->read(data, 1); \
		if (retlen < 0) { \
			syslog(LOG_ERR, "[-NCP-]: Socket error on read: %s %d", \
			       strerror((int)-retlen), (int)(-retlen)); \
			signal_fatal_error(ERRORCODE_ERRNO); \
			goto on_fail; \
		} else if (retlen == 0) { \
			continue; \
		} \
		break ; \
	};

	while (!ncp_state_is_detached_from_ncp(get_ncp_state())) {
		mInboundHeader = 0;
		mInboundFrameSize = 0;

		// Yield until the socket is readable. We do a
		// yield here instead of a wait because we want to
		// only handle one packet per run through the main loop.
		// Using `YIELD` instead of `WAIT` guarantees that we
		// will yield control of the protothread at least once,
		// even if the socket is already readable.
		NLPT_YIELD_UNTIL_READABLE_OR_COND(pt, mSerialAdapter->get_read_fd(), mSerialAdapter->can_read());

#if WPANTUND_SPINEL_USE_FLEN
		do {
			READ_CHARACTER(pt, (void*)&mInboundFrame[0], on_error);

			if (HDLC_BYTE_FLAG != mInboundFrame[0]) {
				// The dreaded extraneous character error.

				// Log the error.
				{
					char printable = mInboundFrame[0];
					if(iscntrl(printable) || printable<0)
						printable = '.';

					syslog(LOG_WARNING,
						   "[NCP->] Extraneous Character: 0x%02X [%c] (%d)\n",
						   (uint8_t)mInboundFrame[0],
						   printable,
						   (uint8_t)mInboundFrame[0]);
				}

				// Flush out all remaining data since this is a strong
				// indication that something has gone horribly wrong.
				while (mSerialAdapter->can_read()) {
					READ_CHARACTER(pt, (void*)&mInboundFrame[0], on_error);
				}

				ncp_is_misbehaving();
				goto on_error;
			}
		} while (UART_STREAM_FLAG != mInboundFrame[0]);

		// Read the frame length
		READ_CHARACTER(pt, (void*)((char*)&mInboundFrame+0), on_error);
		READ_CHARACTER(pt, (void*)((char*)&mInboundFrame+1), on_error);

		mInboundFrameSize = (mInboundFrame[0] << 8) + mInboundFrame[1];

		require(mInboundFrameSize > 1, on_error);
		require(mInboundFrameSize <= SPINEL_FRAME_MAX_SIZE, on_error);

		// Read the rest of the packet.
		NLPT_ASYNC_READ_STREAM(
			pt,
			mSerialAdapter.get(),
			mInboundFrame,
			mInboundFrameSize
		);
#else

		mInboundFrameSize = 0;
		mInboundFrameHDLCCRC = 0xffff;

		do {
			READ_CHARACTER(pt, &byte, on_error);

			if (byte == HDLC_BYTE_FLAG) {
				break;
			}
			if (byte == HDLC_BYTE_ESC) {
				READ_CHARACTER(pt, &byte, on_error);
				if (byte == HDLC_BYTE_FLAG) {
					break;
				} else {
					byte ^= HDLC_ESCAPE_XFORM;
				}
			}

			if (mInboundFrameSize >= 2) {
				mInboundFrameHDLCCRC = hdlc_crc16(mInboundFrameHDLCCRC, mInboundFrame[mInboundFrameSize-2]);
			}

			require(mInboundFrameSize < sizeof(mInboundFrame), on_error);

			mInboundFrame[mInboundFrameSize++] = byte;

		} while(true);

		if (mInboundFrameSize <= 2) {
			continue;
		}

		mInboundFrameSize -= 2;
		mInboundFrameHDLCCRC ^= 0xFFFF;
		{
			uint16_t frame_crc = (mInboundFrame[mInboundFrameSize]|(mInboundFrame[mInboundFrameSize+1]<<8));
			if (mInboundFrameHDLCCRC != frame_crc) {

				int i;
				static const uint8_t kAsciiCR = 13;
				static const uint8_t kAsciiBEL = 7;

				syslog(LOG_ERR, "[NCP->]: Frame CRC Mismatch: Calc:0x%04X != Frame:0x%04X, Garbage on line?", mInboundFrameHDLCCRC, frame_crc);

				// This frame might be an ASCII backtrace, so we check to
				// see if all of the characters are ascii characters, and if
				// so we dump out this packet directly to syslog.

				mInboundFrameSize += 2;

				for (i = 0; i < mInboundFrameSize; i++) {
					// Acceptable control codes
					if (mInboundFrame[i] >= kAsciiBEL && mInboundFrame[i] <= kAsciiCR) {
						continue;
					}
					// NUL characters are OK.
					if (mInboundFrame[i] == 0) {
						continue;
					}
					// Acceptable characters
					if (mInboundFrame[i] >= 32 && mInboundFrame[i] <= 127) {
						continue;
					}

					syslog(LOG_ERR, "[NCP->]: Garbage is not ASCII ([%d]=%d)", i, mInboundFrame[i]);
					break;
				}

				if (i == mInboundFrameSize) {
					handle_ncp_log(mInboundFrame, mInboundFrameSize);
				}

				continue;
			}
		}

#endif

		if (pt->last_errno) {
			syslog(LOG_ERR, "[-NCP-]: Socket error on read: %s", strerror(pt->last_errno));
			errno = pt->last_errno;
			signal_fatal_error(ERRORCODE_ERRNO);
			goto on_error;
		}

		if (spinel_datatype_unpack(mInboundFrame, mInboundFrameSize, "Ci", &mInboundHeader, &command_value) > 0) {
			if ((mInboundHeader&SPINEL_HEADER_FLAG) != SPINEL_HEADER_FLAG) {
				// Unrecognized frame.
				break;
			}

			if (SPINEL_HEADER_GET_IID(mInboundHeader) != 0) {
				// We only support IID zero for now.
				break;
			}

			handle_ncp_spinel_callback(command_value, mInboundFrame, mInboundFrameSize);
		}
	} // while (!ncp_state_is_detached_from_ncp(get_ncp_state()))

on_error:;
	// If we get here, we will restart the protothread at the next iteration.

	NLPT_END(pt);
}

char
SpinelNCPInstance::driver_to_ncp_pump()
{
	struct nlpt*const pt = &mDriverToNCPPumpPT;

	NLPT_BEGIN(pt);

	while (!ncp_state_is_detached_from_ncp(get_ncp_state())) {
		// If there is an outbound callback at this
		// point, then we assume it is stale and
		// immediately clear it out.
		if (!mOutboundCallback.empty()) {
			mOutboundCallback(kWPANTUNDStatus_Canceled);
			mOutboundCallback.clear();
		}

		// Wait for a packet to be available from interface OR management queue.
		if (mOutboundBufferLen > 0) {
			// If there is something in the outbound queue,
			// we shouldn't try any of the checks below, since it
			// will delay processing.

		} else if (static_cast<bool>(mLegacyInterface) && is_legacy_interface_enabled()) {
			NLPT_YIELD_UNTIL_READABLE2_OR_COND(
				pt,
				mPrimaryInterface->get_read_fd(),
				mLegacyInterface->get_read_fd(),
				(mOutboundBufferLen > 0)
				|| mLegacyInterface->can_read()
				|| mPrimaryInterface->can_read()
			);

		} else {
			NLPT_YIELD_UNTIL_READABLE_OR_COND(
				pt,
				mPrimaryInterface->get_read_fd(),
				mPrimaryInterface->can_read() || (mOutboundBufferLen > 0)
			);
		}

		// Get packet or management command, and also
		// perform any necessary filtering.
		if (mOutboundBufferLen > 0) {
			if (mOutboundBuffer[1] == SPINEL_CMD_PROP_VALUE_GET) {
				spinel_prop_key_t key;
				spinel_datatype_unpack(mOutboundBuffer, mOutboundBufferLen, "Cii", NULL, NULL, &key);
				syslog(LOG_INFO, "[->NCP] CMD_PROP_VALUE_GET(%s) tid:%d", spinel_prop_key_to_cstr(key), SPINEL_HEADER_GET_TID(mOutboundBuffer[0]));
			} else if (mOutboundBuffer[1] == SPINEL_CMD_PROP_VALUE_SET) {
				spinel_prop_key_t key;
				spinel_datatype_unpack(mOutboundBuffer, mOutboundBufferLen, "Cii", NULL, NULL, &key);
				syslog(LOG_INFO, "[->NCP] CMD_PROP_VALUE_SET(%s) tid:%d", spinel_prop_key_to_cstr(key), SPINEL_HEADER_GET_TID(mOutboundBuffer[0]));
			} else if (mOutboundBuffer[1] == SPINEL_CMD_PROP_VALUE_INSERT) {
				spinel_prop_key_t key;
				spinel_datatype_unpack(mOutboundBuffer, mOutboundBufferLen, "Cii", NULL, NULL, &key);
				syslog(LOG_INFO, "[->NCP] CMD_PROP_VALUE_INSERT(%s) tid:%d", spinel_prop_key_to_cstr(key), SPINEL_HEADER_GET_TID(mOutboundBuffer[0]));
			} else if (mOutboundBuffer[1] == SPINEL_CMD_PROP_VALUE_REMOVE) {
				spinel_prop_key_t key;
				spinel_datatype_unpack(mOutboundBuffer, mOutboundBufferLen, "Cii", NULL, NULL, &key);
				syslog(LOG_INFO, "[->NCP] CMD_PROP_VALUE_REMOVE(%s) tid:%d", spinel_prop_key_to_cstr(key), SPINEL_HEADER_GET_TID(mOutboundBuffer[0]));
			} else if (mOutboundBuffer[1] == SPINEL_CMD_NOOP) {
				syslog(LOG_INFO, "[->NCP] CMD_NOOP tid:%d", SPINEL_HEADER_GET_TID(mOutboundBuffer[0]));
			} else if (mOutboundBuffer[1] == SPINEL_CMD_RESET) {
				syslog(LOG_INFO, "[->NCP] CMD_RESET tid:%d", SPINEL_HEADER_GET_TID(mOutboundBuffer[0]));
			} else if (mOutboundBuffer[1] == SPINEL_CMD_NET_CLEAR) {
				syslog(LOG_INFO, "[->NCP] CMD_NET_CLEAR tid:%d", SPINEL_HEADER_GET_TID(mOutboundBuffer[0]));
			} else {
				syslog(LOG_INFO, "[->NCP] Spinel command 0x%02X tid:%d", mOutboundBuffer[1], SPINEL_HEADER_GET_TID(mOutboundBuffer[0]));
			}
		} else {
			// There is an IPv6 packet waiting on one of the tunnel interfaces.

			if (mPrimaryInterface->can_read()) {
				mOutboundBufferLen = (spinel_ssize_t)mPrimaryInterface->read(
					&mOutboundBuffer[5],
					sizeof(mOutboundBuffer)-5
				);
				mOutboundBufferType = FRAME_TYPE_DATA;
			} else if (static_cast<bool>(mLegacyInterface)) {
				mOutboundBufferLen = (spinel_ssize_t)mLegacyInterface->read(
					&mOutboundBuffer[5],
					sizeof(mOutboundBuffer)-5
				);
				mOutboundBufferType = FRAME_TYPE_LEGACY_DATA;
			}

			if (0 > mOutboundBufferLen) {
				syslog(LOG_ERR,
				       "driver_to_ncp_pump: Socket error on read: %s",
				       strerror(errno));
				signal_fatal_error(ERRORCODE_ERRNO);
				break;
			}

			if (mOutboundBufferLen <= 0) {
				// No packet...?
				mOutboundBufferLen = 0;
				continue;
			}

			if (!should_forward_ncpbound_frame(&mOutboundBufferType, &mOutboundBuffer[5], mOutboundBufferLen)) {
				mOutboundBufferLen = 0;
				continue;
			}

			if (get_ncp_state() == CREDENTIALS_NEEDED) {
				mOutboundBufferType = FRAME_TYPE_INSECURE_DATA;
			}

			mOutboundBuffer[3] = (mOutboundBufferLen & 0xFF);
			mOutboundBuffer[4] = ((mOutboundBufferLen >> 8) & 0xFF);

			mOutboundBufferLen += 5;

			mOutboundBuffer[0] = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID_0;
			mOutboundBuffer[1] = SPINEL_CMD_PROP_VALUE_SET;

			if (mOutboundBufferType == FRAME_TYPE_DATA) {
				mOutboundBuffer[2] = SPINEL_PROP_STREAM_NET;

			} else if (mOutboundBufferType == FRAME_TYPE_INSECURE_DATA) {
				mOutboundBuffer[2] = SPINEL_PROP_STREAM_NET_INSECURE;

			} else {
				mOutboundBuffer[0] = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID_1;
				mOutboundBuffer[2] = SPINEL_PROP_STREAM_NET;
			}
		}

#if VERBOSE_DEBUG || 1
		// Very verbose debugging. Dumps out all outbound packets.
		{
			char readable_buffer[300];
			encode_data_into_string(mOutboundBuffer,
			                        mOutboundBufferLen,
			                        readable_buffer,
			                        sizeof(readable_buffer),
			                        0);
			syslog(LOG_INFO, "\t↳ %s", (const char*)readable_buffer);
		}
#endif // VERBOSE_DEBUG


#if WPANTUND_SPINEL_USE_FLEN
		mOutboundBufferHeader[0] = HDLC_BYTE_FLAG;
		mOutboundBufferHeader[1] = (mOutboundBufferLen >> 8);
		mOutboundBufferHeader[2] = (mOutboundBufferLen & 0xFF);

		mOutboundBufferSent = 0;

		// Go ahead send
		NLPT_ASYNC_WRITE_STREAM(
			pt,
			mSerialAdapter.get(),
			mOutboundBufferHeader,
			mOutboundBufferLen + sizeof(mOutboundBufferHeader)
		);
		mOutboundBufferSent += pt->byte_count;
#else

		mOutboundBufferEscapedLen = 1;
		mOutboundBufferEscaped[0] = HDLC_BYTE_FLAG;
		{
			spinel_ssize_t i;
			uint8_t byte;
			uint16_t crc(0xFFFF);
			for (i = 0; i < mOutboundBufferLen; i++) {
				byte = mOutboundBuffer[i];
				crc = hdlc_crc16(crc, byte);
				if (hdlc_byte_needs_escape(byte)) {
					mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = HDLC_BYTE_ESC;
					mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = byte ^ HDLC_ESCAPE_XFORM;
				} else {
					mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = byte;
				}
			}
			crc ^= 0xFFFF;
			byte = (crc & 0xFF);
			if (hdlc_byte_needs_escape(byte)) {
				mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = HDLC_BYTE_ESC;
				mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = byte ^ HDLC_ESCAPE_XFORM;
			} else {
				mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = byte;
			}
			byte = ((crc>>8) & 0xFF);
			if (hdlc_byte_needs_escape(byte)) {
				mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = HDLC_BYTE_ESC;
				mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = byte ^ HDLC_ESCAPE_XFORM;
			} else {
				mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = byte;
			}
			mOutboundBufferEscaped[mOutboundBufferEscapedLen++] = HDLC_BYTE_FLAG;
		}

		mOutboundBufferSent = 0;

		// Go ahead send
		NLPT_ASYNC_WRITE_STREAM(
			pt,
			mSerialAdapter.get(),
			mOutboundBufferEscaped,
			mOutboundBufferEscapedLen
		);
		mOutboundBufferSent += pt->byte_count;
#endif

		mOutboundBufferLen = 0;

		require(pt->last_errno == 0, on_error);

		// Go ahead and fire off the "did send" callback.
		if (!mOutboundCallback.empty()) {
			mOutboundCallback(kWPANTUNDStatus_Ok);
			mOutboundCallback.clear();
		}

	} // while(true)

on_error:
	// If we get here, we will restart the protothread at the next iteration.

	if (!mOutboundCallback.empty()) {
		mOutboundCallback(kWPANTUNDStatus_Failure);
		mOutboundCallback.clear();
	}

	NLPT_END(pt);
}
