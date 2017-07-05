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
 *    Description:
 *		Abstract base class for NCP implementations.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "NCPControlInterface.h"
#include "NCPInstance.h"
#include "tunnel.h"
#include <syslog.h>
#include <arpa/inet.h>
#include <errno.h>
#include <boost/bind.hpp>
#include "version.h"
#include "any-to.h"
#include "wpan-error.h"

#ifndef SOURCE_VERSION
#define SOURCE_VERSION		PACKAGE_VERSION
#endif

using namespace nl;
using namespace wpantund;

std::string
NCPControlInterface::external_route_priority_to_string(ExternalRoutePriority route_priority)
{
	const char *ret = "unknown";

	switch(route_priority) {
		case ROUTE_LOW_PREFRENCE:
			ret = "low";
			break;

		case ROUTE_MEDIUM_PREFERENCE:
			ret = "medium(normal)";
			break;

		case ROUTE_HIGH_PREFERENCE:
			ret = "high";
			break;
	}

	return ret;
}

NCPControlInterface::~NCPControlInterface() {
}

struct GetPropertyHelper {
	boost::any *dest;
	bool *          didFire;

	void operator()(
	    int status, const boost::any& value
	    ) const
	{
		if (dest) {
			if (status == 0)
				*dest = value;
			*didFire = true;
		}
		delete this;
	}
};

boost::any
NCPControlInterface::property_get_value(const std::string& key)
{
	// In this function, we want to immediately return the value
	// for the given key. Since the actual property_get_value()
	// function returns the value as a callback, we may not
	// actually be able to do this. What we do here is give a
	// callback object that contains a pointer to our return value.
	// After we call property_get_value(), we then zero out that
	// pointer. For properties which get updated immediately, our
	// return value will be updated automatically. For properties
	// which can't be fetched immediately, we return an empty value.
	// The "GetPropertyHelper" class makes sure that when the
	// callback eventually does fire that it doesn't break anything.

	boost::any ret;
	bool didFire = false;
	struct GetPropertyHelper *helper = new GetPropertyHelper;

	helper->dest = &ret;
	helper->didFire = &didFire;
	property_get_value(key, boost::bind(&GetPropertyHelper::operator(),
	                              helper,
	                              _1,
	                              _2));
	if (!didFire) {
		helper->dest = 0;
		helper->didFire = 0;
	}
	return ret;
}

std::string
NCPControlInterface::get_name() {
	return boost::any_cast<std::string>(property_get_value(kWPANTUNDProperty_ConfigTUNInterfaceName));
}


struct SetPropertyHelper {
	int* dest;
	bool* didFire;

	void operator()(int status) const
	{
		if (dest) {
			*dest = status;
			*didFire = true;
		}
		delete this;
	}
};

#define kWPANTUNDPropertyNCPSocketName             "NCPSocketName"
#define kWPANTUNDPropertyNCPSocketBaud             "NCPSocketBaud"
#define kWPANTUNDPropertyNCPDriverName             "NCPDriverName"
#define kWPANTUNDPropertyNCPHardResetPath          "NCPHardResetPath"
#define kWPANTUNDPropertyNCPPowerPath              "NCPPowerPath"
#define kWPANTUNDPropertyWPANInterfaceName         "WPANInterfaceName"
#define kWPANTUNDPropertyPIDFile                   "PIDFile"
#define kWPANTUNDPropertyFirmwareCheckCommand      "FirmwareCheckCommand"
#define kWPANTUNDPropertyFirmwareUpgradeCommand    "FirmwareUpgradeCommand"
#define kWPANTUNDPropertyTerminateOnFault          "TerminateOnFault"
#define kWPANTUNDPropertyPrivDropToUser            "PrivDropToUser"
#define kWPANTUNDPropertyChroot                    "Chroot"
#define kWPANTUNDPropertyNCPReliabilityLayer       "NCPReliabilityLayer"

// Version Properties
#define kWPANTUNDPropertyNCPVersion                "NCPVersion"
#define kWPANTUNDPropertyDriverVersion             "DriverVersion"

// Driver State Properties
#define kWPANTUNDPropertyAssociationState          "AssociationState"    // [RO]
#define kWPANTUNDPropertyEnabled                   "Enabled"             // [RW]
#define kWPANTUNDPropertyAutoresume                "AutoResume"          // [RW]
#define kWPANTUNDPropertyAutoUpdateFirmware        "AutoUpdateFirmware"  // [RW]

// PHY-layer parameters
#define kWPANTUNDPropertyHWAddr                    "HWAddr"
#define kWPANTUNDPropertyChannel                   "Channel"
#define kWPANTUNDPropertyTXPower                   "TXPower"
#define kWPANTUNDPropertyNCPTXPowerLimit           "NCPTXPowerLimit"
#define kWPANTUNDPropertyCCAThreshold              "CCAThreshold"
#define kWPANTUNDPropertyDefaultChannelMask        "DefaultChannelMask"

// MAC-layer (and higher) parameters
#define kWPANTUNDPropertyNetworkName               "NetworkName"         // [RO]
#define kWPANTUNDPropertyXPANID                    "XPANID"              // [RO]
#define kWPANTUNDPropertyPANID                     "PANID"               // [RO]
#define kWPANTUNDPropertyNodeType                  "NodeType"            // [RW]
#define kWPANTUNDPropertyNetworkKey                "NetworkKey"          // [RW]
#define kWPANTUNDPropertyNetworkKeyIndex           "NetworkKeyIndex"     // [RW]
#define kWPANTUNDPropertyMeshLocalPrefix           "MeshLocalPrefix"     // [RO]
#define kWPANTUNDPropertyAllowingJoin              "AllowingJoin"        // [RO]
#define kWPANTUNDPropertyIsAssociated              "IsAssociated"        // [RO]

// Power Management Properties
#define kWPANTUNDPropertyIsOKToSleep               "IsOKToSleep"
#define kWPANTUNDPropertyUseDeepSleepOnLowPower    "UseDeepSleepOnLowPower"
#define kWPANTUNDPropertyAlwaysResetToWake         "AlwaysResetToWake"
#define kWPANTUNDPropertyAutoDeepSleep             "AutoDeepSleep"
#define kWPANTUNDPropertySleepPollInterval         "SleepPollInterval"

// Debugging and logging
#define kWPANTUNDPropertySyslogMask                "SyslogMask"
#define kWPANTUNDPropertyNCPDebug                  "NCPDebug"

// Properties related to manufacturing test commands
#define kWPANTUNDPropertyMfgTestMode               "MfgTestMode"
#define kWPANTUNDPropertyMfgSYNOffset              "MfgSYNOffset"
#define kWPANTUNDPropertyMfgRepeatRandomTXInterval "MfgRepeatRandomTXInterval"
#define kWPANTUNDPropertyMfgRepeatRandomTXLen      "MfgRepeatRandomTXLen"
#define kWPANTUNDPropertyMfgFirstPacketRSSI        "MfgFirstPacketRSSI"
#define kWPANTUNDPropertyMfgFirstPacketLQI         "MfgFirstPacketLQI"


// Nest-Specific Properties
#define kWPANTUNDPropertyPassthruPort              "PassthruPort"
#define kWPANTUNDPropertyTransmitHookActive        "TransmitHookActive"
#define kWPANTUNDPropertyUseLegacyChannel          "UseLegacyChannel"
#define kWPANTUNDPropertyLegacyPrefix              "LegacyPrefix"
#define kWPANTUNDPropertyNetWakeData               "NetWakeData"
#define kWPANTUNDPropertyNetWakeRemaining          "NetWakeRemaining"
#define kWPANTUNDPropertyActiveWakeupBlacklist     "ActiveWakeupBlacklist"
#define kWPANTUNDPropertyLegacyInterfaceEnabled    "LegacyInterfaceEnabled"
#define kWPANTUNDPropertyPrefix                    "Prefix"

#define kWPANTUNDPropertyGlobalIPAddresses         "GlobalIPAddresses"
#define kWPANTUNDPropertyGlobalIPAddressList       "GlobalIPAddressList"

int
NCPControlInterface::property_set_value(const std::string& key, const boost::any& value)
{
	// In this function, we want to immediately return the status
	// of the set operation. Since the actual property_set_value()
	// function returns the status as a callback, we may not
	// actually be able to do this. What we do here is give a
	// callback object that contains a pointer to our return value.
	// After we call property_set_value(), we then zero out that
	// pointer. For properties which get updated immediately, our
	// return value will be updated automatically. For properties
	// which can't be fetched immediately, we return -EINPROGRESS.
	// The "SetPropertyHelper" class makes sure that when the
	// callback eventually does fire that it doesn't break anything.

	int ret = kWPANTUNDStatus_InProgress;
	bool didFire = false;
	struct SetPropertyHelper *helper = new SetPropertyHelper;

	helper->dest = &ret;
	helper->didFire = &didFire;
	property_set_value(key, value, boost::bind(&SetPropertyHelper::operator(),
	                              helper,
	                              _1));
	if (!didFire) {
		helper->dest = 0;
		helper->didFire = 0;
	}
	return ret;
}


bool
NCPControlInterface::translate_deprecated_property(std::string& key, boost::any& value)
{
	bool ret = false;
	if (strcaseequal(key.c_str(), kWPANTUNDPropertyPrefix)) {
		key = kWPANTUNDProperty_IPv6MeshLocalPrefix;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNCPSocketName )) {
		key =  kWPANTUNDProperty_ConfigNCPSocketPath ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNCPSocketBaud )) {
		key =  kWPANTUNDProperty_ConfigNCPSocketBaud ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNCPDriverName )) {
		key =  kWPANTUNDProperty_ConfigNCPDriverName ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNCPHardResetPath )) {
		key =  kWPANTUNDProperty_ConfigNCPHardResetPath ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNCPPowerPath )) {
		key =  kWPANTUNDProperty_ConfigNCPPowerPath ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyWPANInterfaceName )) {
		key =  kWPANTUNDProperty_ConfigTUNInterfaceName ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyPIDFile )) {
		key =  kWPANTUNDProperty_ConfigDaemonPIDFile ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyFirmwareCheckCommand )) {
		key =  kWPANTUNDProperty_ConfigNCPFirmwareCheckCommand ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyFirmwareUpgradeCommand )) {
		key =  kWPANTUNDProperty_ConfigNCPFirmwareUpgradeCommand ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyTerminateOnFault )) {
		key =  kWPANTUNDProperty_DaemonTerminateOnFault ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyPrivDropToUser )) {
		key =  kWPANTUNDProperty_ConfigDaemonPrivDropToUser ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyChroot )) {
		key =  kWPANTUNDProperty_ConfigDaemonChroot ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNCPReliabilityLayer )) {
		key =  kWPANTUNDProperty_ConfigNCPReliabilityLayer ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNCPVersion )) {
		key =  kWPANTUNDProperty_NCPVersion ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyDriverVersion )) {
		key =  kWPANTUNDProperty_DaemonVersion ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyAssociationState )) {
		key =  kWPANTUNDProperty_NCPState ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyEnabled )) {
		key =  kWPANTUNDProperty_DaemonEnabled ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyAutoresume )) {
		key =  kWPANTUNDProperty_DaemonAutoAssociateAfterReset ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyAutoUpdateFirmware )) {
		key =  kWPANTUNDProperty_DaemonAutoFirmwareUpdate ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyHWAddr )) {
		key =  kWPANTUNDProperty_NCPHardwareAddress ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyChannel )) {
		key =  kWPANTUNDProperty_NCPChannel ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyTXPower )) {
		key =  kWPANTUNDProperty_NCPTXPower ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNCPTXPowerLimit )) {
		key =  kWPANTUNDProperty_NCPTXPowerLimit ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyCCAThreshold )) {
		key =  kWPANTUNDProperty_NCPCCAThreshold ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyDefaultChannelMask )) {
		key =  kWPANTUNDProperty_NCPChannelMask ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNetworkName )) {
		key =  kWPANTUNDProperty_NetworkName ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyXPANID )) {
		key =  kWPANTUNDProperty_NetworkXPANID ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyPANID )) {
		key =  kWPANTUNDProperty_NetworkPANID ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNodeType )) {
		key =  kWPANTUNDProperty_NetworkNodeType ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNetworkKey )) {
		key =  kWPANTUNDProperty_NetworkKey ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNetworkKeyIndex )) {
		key =  kWPANTUNDProperty_NetworkKeyIndex ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyMeshLocalPrefix )) {
		key =  kWPANTUNDProperty_IPv6MeshLocalPrefix ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyAllowingJoin )) {
		key =  kWPANTUNDProperty_NestLabs_NetworkAllowingJoin ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyIsAssociated )) {
		key =  kWPANTUNDProperty_NetworkIsCommissioned ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyIsOKToSleep )) {
		key =  kWPANTUNDProperty_DaemonReadyForHostSleep ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyUseDeepSleepOnLowPower )) {
		key =  kWPANTUNDProperty_NestLabs_HackUseDeepSleepOnLowPower ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyAlwaysResetToWake )) {
		key =  kWPANTUNDProperty_NestLabs_HackAlwaysResetToWake ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyAutoDeepSleep )) {
		key =  kWPANTUNDProperty_DaemonAutoDeepSleep ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertySleepPollInterval )) {
		key =  kWPANTUNDProperty_NCPSleepyPollInterval ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertySyslogMask )) {
		key =  kWPANTUNDProperty_DaemonSyslogMask ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyPassthruPort )) {
		key =  kWPANTUNDProperty_NestLabs_NetworkPassthruPort ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyTransmitHookActive )) {
		key =  kWPANTUNDProperty_NestLabs_NCPTransmitHookActive ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyLegacyPrefix )) {
		key =  kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNetWakeData )) {
		key =  kWPANTUNDProperty_NestLabs_NetworkWakeData ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyNetWakeRemaining )) {
		key =  kWPANTUNDProperty_NestLabs_NetworkWakeRemaining ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyActiveWakeupBlacklist ) || strcaseequal(key.c_str(), "ActiveWakeupMask")) {
		key =  kWPANTUNDProperty_NestLabs_NetworkWakeBlacklist ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyLegacyInterfaceEnabled )) {
		key =  kWPANTUNDProperty_NestLabs_LegacyEnabled ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyUseLegacyChannel )) {
		key =  kWPANTUNDProperty_NestLabs_LegacyPreferInterface ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyGlobalIPAddresses )) {
		key =  kWPANTUNDProperty_IPv6AllAddresses ;
		ret = true;
	} else if (strcaseequal(key.c_str(),  kWPANTUNDPropertyGlobalIPAddressList )) {
		key =  kWPANTUNDProperty_DebugIPv6GlobalIPAddressList ;
		ret = true;
	}
	return ret;
}

bool
NCPControlInterface::translate_deprecated_property(std::string& key)
{
	boost::any unused;
	return translate_deprecated_property(key, unused);
}
