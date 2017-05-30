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

#define _GNU_SOURCE 1

#include "assert-macros.h"
#include "wpan-error.h"
#include <errno.h>
#include <string.h>

const char* wpantund_status_to_cstr(int status)
{
	if (status < 0) {
		return strerror(status);
	}

	if (WPANTUND_STATUS_IS_NCPERROR(status)) {
		return "NCP-Specific Errorcode";
	}

	switch(status) {
	case kWPANTUNDStatus_Ok: return "Ok";
	case kWPANTUNDStatus_Failure: return "Failure";
	case kWPANTUNDStatus_Timeout: return "Timeout";
	case kWPANTUNDStatus_SocketReset: return "SocketReset";
	case kWPANTUNDStatus_InvalidArgument: return "InvalidArgument";
	case kWPANTUNDStatus_Busy: return "Busy";
	case kWPANTUNDStatus_InvalidWhenDisabled: return "InvalidWhenDisabled";
	case kWPANTUNDStatus_InvalidForCurrentState: return "InvalidForCurrentState";
	case kWPANTUNDStatus_PropertyEmpty: return "PropertyEmpty";
	case kWPANTUNDStatus_InvalidType: return "InvalidType";
	case kWPANTUNDStatus_FeatureNotSupported: return "FeatureNotSupported";
	case kWPANTUNDStatus_FeatureNotImplemented: return "FeatureNotImplemented";
	case kWPANTUNDStatus_PropertyNotFound: return "PropertyNotFound";
	case kWPANTUNDStatus_Canceled: return "Canceled";
	case kWPANTUNDStatus_InProgress: return "InProgress";
	case kWPANTUNDStatus_JoinFailedUnknown: return "JoinFailedUnknown";
	case kWPANTUNDStatus_JoinFailedAtScan: return "JoinFailedAtScan";
	case kWPANTUNDStatus_JoinFailedAtAuthenticate: return "JoinFailedAtAuthenticate";
	case kWPANTUNDStatus_FormFailedAtScan: return "FormFailedAtScan";
	case kWPANTUNDStatus_Already: return "Already";
	case kWPANTUNDStatus_TryAgainLater: return "TryAgainLater";
	case kWPANTUNDStatus_InvalidRange: return "InvalidRange";
	case kWPANTUNDStatus_InterfaceNotFound: return "InterfaceNotFound";
	case kWPANTUNDStatus_NCP_Crashed: return "NCPCrashed";
	case kWPANTUNDStatus_NCP_Fatal: return "NCPFatal";
	case kWPANTUNDStatus_NCP_InvalidArgument: return "NCPInvalidArgument";
	case kWPANTUNDStatus_NCP_InvalidRange: return "NCPInvalidRange";
	case kWPANTUNDStatus_NCP_Reset: return "NCPReset";
	case kWPANTUNDStatus_MissingXPANID: return "MissingXPANID";
	default: break;
	}
	return "";
}
