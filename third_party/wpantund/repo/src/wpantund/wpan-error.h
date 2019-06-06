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
 */

#ifndef wpantund_wpan_error_h
#define wpantund_wpan_error_h
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
	kWPANTUNDStatus_Ok                            = 0,
	kWPANTUNDStatus_Failure                       = 1,

	kWPANTUNDStatus_InvalidArgument               = 2,
	kWPANTUNDStatus_InvalidWhenDisabled           = 3,
	kWPANTUNDStatus_InvalidForCurrentState        = 4,
	kWPANTUNDStatus_InvalidType                   = 5,
	kWPANTUNDStatus_InvalidRange                  = 6,

	kWPANTUNDStatus_Timeout                       = 7,
	kWPANTUNDStatus_SocketReset                   = 8,
	kWPANTUNDStatus_Busy                          = 9,

	kWPANTUNDStatus_Already                       = 10,
	kWPANTUNDStatus_Canceled                      = 11,
	kWPANTUNDStatus_InProgress                    = 12,
	kWPANTUNDStatus_TryAgainLater                 = 13,

	kWPANTUNDStatus_FeatureNotSupported           = 14,
	kWPANTUNDStatus_FeatureNotImplemented         = 15,

	kWPANTUNDStatus_PropertyNotFound              = 16,
	kWPANTUNDStatus_PropertyEmpty                 = 17,

	kWPANTUNDStatus_JoinFailedUnknown             = 18,
	kWPANTUNDStatus_JoinFailedAtScan              = 19,
	kWPANTUNDStatus_JoinFailedAtAuthenticate      = 20,
	kWPANTUNDStatus_FormFailedAtScan              = 21,

	kWPANTUNDStatus_NCP_Crashed                   = 22,
	kWPANTUNDStatus_NCP_Fatal                     = 23,
	kWPANTUNDStatus_NCP_InvalidArgument           = 24,
	kWPANTUNDStatus_NCP_InvalidRange              = 25,

	kWPANTUNDStatus_MissingXPANID                 = 26,

	kWPANTUNDStatus_NCP_Reset                     = 27,

	kWPANTUNDStatus_InterfaceNotFound             = 28,

	kWPANTUNDStatus_JoinerFailed_Security         = 29,
	kWPANTUNDStatus_JoinerFailed_NoPeers          = 30,
	kWPANTUNDStatus_JoinerFailed_ResponseTimeout  = 31,
	kWPANTUNDStatus_JoinerFailed_Unknown          = 32,

	kWPANTUNDStatus_NCPError_First                = 0xEA0000,
	kWPANTUNDStatus_NCPError_Last                 = 0xEAFFFF,
} wpantund_status_t;

#define WPANTUND_NCPERROR_MASK		0xFFFF

#define WPANTUND_STATUS_IS_NCPERROR(x)	((((int)x)&~WPANTUND_NCPERROR_MASK) == kWPANTUNDStatus_NCPError_First)
#define WPANTUND_NCPERROR_TO_STATUS(x)	(wpantund_status_t)((((int)x)&WPANTUND_NCPERROR_MASK)|kWPANTUNDStatus_NCPError_First)
#define WPANTUND_STATUS_TO_NCPERROR(x)	((x)&WPANTUND_NCPERROR_MASK)

extern const char* wpantund_status_to_cstr(int status);

#if defined(__cplusplus)
}
#endif

#endif
