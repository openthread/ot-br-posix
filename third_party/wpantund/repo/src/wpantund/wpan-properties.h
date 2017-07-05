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
 *      This file contains the enumeration of the properties that can be
 *      gotten or set via the "get_prop()" and "set_prop()" methods.
 *
 */

#ifndef wpantund_wpan_properties_h
#define wpantund_wpan_properties_h

#define kWPANTUNDProperty_ConfigNCPSocketPath                   "Config:NCP:SocketPath"
#define kWPANTUNDProperty_ConfigNCPSocketBaud                   "Config:NCP:SocketBaud"
#define kWPANTUNDProperty_ConfigNCPDriverName                   "Config:NCP:DriverName"
#define kWPANTUNDProperty_ConfigNCPHardResetPath                "Config:NCP:HardResetPath"
#define kWPANTUNDProperty_ConfigNCPPowerPath                    "Config:NCP:PowerPath"
#define kWPANTUNDProperty_ConfigNCPReliabilityLayer             "Config:NCP:ReliabilityLayer"
#define kWPANTUNDProperty_ConfigNCPFirmwareCheckCommand         "Config:NCP:FirmwareCheckCommand"
#define kWPANTUNDProperty_ConfigNCPFirmwareUpgradeCommand       "Config:NCP:FirmwareUpgradeCommand"
#define kWPANTUNDProperty_ConfigTUNInterfaceName                "Config:TUN:InterfaceName"
#define kWPANTUNDProperty_ConfigDaemonPIDFile                   "Config:Daemon:PIDFile"
#define kWPANTUNDProperty_ConfigDaemonPrivDropToUser            "Config:Daemon:PrivDropToUser"
#define kWPANTUNDProperty_ConfigDaemonChroot                    "Config:Daemon:Chroot"
#define kWPANTUNDProperty_ConfigDaemonNetworkRetainCommand      "Config:Daemon:NetworkRetainCommand"

#define kWPANTUNDProperty_DaemonVersion                         "Daemon:Version"
#define kWPANTUNDProperty_DaemonEnabled                         "Daemon:Enabled"
#define kWPANTUNDProperty_DaemonSyslogMask                      "Daemon:SyslogMask"
#define kWPANTUNDProperty_DaemonTerminateOnFault                "Daemon:TerminateOnFault"
#define kWPANTUNDProperty_DaemonReadyForHostSleep               "Daemon:ReadyForHostSleep"
#define kWPANTUNDProperty_DaemonAutoAssociateAfterReset         "Daemon:AutoAssociateAfterReset"
#define kWPANTUNDProperty_DaemonAutoFirmwareUpdate              "Daemon:AutoFirmwareUpdate"
#define kWPANTUNDProperty_DaemonAutoDeepSleep                   "Daemon:AutoDeepSleep"
#define kWPANTUNDProperty_DaemonFaultReason                     "Daemon:FaultReason"

#define kWPANTUNDProperty_NCPVersion                            "NCP:Version"
#define kWPANTUNDProperty_NCPState                              "NCP:State"
#define kWPANTUNDProperty_NCPHardwareAddress                    "NCP:HardwareAddress"
#define kWPANTUNDProperty_NCPExtendedAddress                    "NCP:ExtendedAddress"
#define kWPANTUNDProperty_NCPMACAddress                         "NCP:MACAddress"
#define kWPANTUNDProperty_NCPChannel                            "NCP:Channel"
#define kWPANTUNDProperty_NCPFrequency                          "NCP:Frequency"
#define kWPANTUNDProperty_NCPTXPower                            "NCP:TXPower"
#define kWPANTUNDProperty_NCPTXPowerLimit                       "NCP:TXPowerLimit"
#define kWPANTUNDProperty_NCPCCAThreshold                       "NCP:CCAThreshold"
#define kWPANTUNDProperty_NCPChannelMask                        "NCP:ChannelMask"
#define kWPANTUNDProperty_NCPSleepyPollInterval                 "NCP:SleepyPollInterval"
#define kWPANTUNDProperty_NCPRSSI                               "NCP:RSSI"

#define kWPANTUNDProperty_InterfaceUp                           "Interface:Up"

#define kWPANTUNDProperty_NetworkName                           "Network:Name"
#define kWPANTUNDProperty_NetworkXPANID                         "Network:XPANID"
#define kWPANTUNDProperty_NetworkPANID                          "Network:PANID"
#define kWPANTUNDProperty_NetworkNodeType                       "Network:NodeType"
#define kWPANTUNDProperty_NetworkKey                            "Network:Key"
#define kWPANTUNDProperty_NetworkKeyIndex                       "Network:KeyIndex"
#define kWPANTUNDProperty_NetworkIsCommissioned                 "Network:IsCommissioned"
#define kWPANTUNDProperty_NetworkIsConnected                    "Network:IsConnected"
#define kWPANTUNDProperty_NetworkPSKc                           "Network:PSKc"
#define kWPANTUNDProperty_NetworkRole                           "Network:Role"
#define kWPANTUNDProperty_NetworkPartitionId                    "Network:PartitionId"

#define kWPANTUNDProperty_IPv6LinkLocalAddress                  "IPv6:LinkLocalAddress"
#define kWPANTUNDProperty_IPv6MeshLocalAddress                  "IPv6:MeshLocalAddress"
#define kWPANTUNDProperty_IPv6MeshLocalPrefix                   "IPv6:MeshLocalPrefix"
#define kWPANTUNDProperty_IPv6AllAddresses                      "IPv6:AllAddresses"

#define kWPANTUNDProperty_ThreadRLOC16                          "Thread:RLOC16"
#define kWPANTUNDProperty_ThreadRouterID                        "Thread:RouterID"
#define kWPANTUNDProperty_ThreadLeaderAddress                   "Thread:Leader:Address"
#define kWPANTUNDProperty_ThreadLeaderRouterID                  "Thread:Leader:RouterID"
#define kWPANTUNDProperty_ThreadLeaderWeight                    "Thread:Leader:Weight"
#define kWPANTUNDProperty_ThreadLeaderLocalWeight               "Thread:Leader:LocalWeight"
#define kWPANTUNDProperty_ThreadLeaderNetworkData               "Thread:Leader:NetworkData"
#define kWPANTUNDProperty_ThreadStableLeaderNetworkData         "Thread:Leader:StableNetworkData"
#define kWPANTUNDProperty_ThreadNetworkData                     "Thread:NetworkData"
#define kWPANTUNDProperty_ThreadChildTable                      "Thread:ChildTable"
#define kWPANTUNDProperty_ThreadChildTableAsValMap              "Thread:ChildTable:AsValMap"
#define kWPANTUNDProperty_ThreadNeighborTable                   "Thread:NeighborTable"
#define kWPANTUNDProperty_ThreadNeighborTableAsValMap           "Thread:NeighborTable:AsValMap"
#define kWPANTUNDProperty_ThreadNetworkDataVersion              "Thread:NetworkDataVersion"
#define kWPANTUNDProperty_ThreadStableNetworkData               "Thread:StableNetworkData"
#define kWPANTUNDProperty_ThreadStableNetworkDataVersion        "Thread:StableNetworkDataVersion"
#define kWPANTUNDProperty_ThreadPreferredRouterID               "Thread:PreferredRouterID"
#define kWPANTUNDProperty_ThreadCommissionerEnabled             "Thread:Commissioner:Enabled"
#define kWPANTUNDProperty_ThreadDeviceMode                      "Thread:DeviceMode"
#define kWPANTUNDProperty_ThreadOffMeshRoutes                   "Thread:OffMeshRoutes"
#define kWPANTUNDProperty_ThreadOnMeshPrefixes                  "Thread:OnMeshPrefixes"

#define kWPANTUNDProperty_OpenThreadLogLevel                    "OpenThread:LogLevel"
#define kWPANTUNDProperty_OpenThreadSteeringDataAddress         "OpenThread:SteeringData:Address"
#define kWPANTUNDProperty_OpenThreadSteeringDataSetWhenJoinable "OpenThread:SteeringData:SetWhenJoinable"
#define kWPANTUNDProperty_OpenThreadMsgBufferCounters           "OpenThread:MsgBufferCounters"
#define kWPANTUNDProperty_OpenThreadMsgBufferCountersAsString   "OpenThread:MsgBufferCounters:AsString"
#define kWPANTUNDProperty_OpenThreadDebugTestAssert             "OpenThread:Debug:TestAssert"

#define kWPANTUNDProperty_DebugIPv6GlobalIPAddressList          "Debug:IPv6:GlobalIPAddressList"

#define kWPANTUNDProperty_MACWhitelistEnabled                   "MAC:Whitelist:Enabled"
#define kWPANTUNDProperty_MACWhitelistEntries                   "MAC:Whitelist:Entries"
#define kWPANTUNDProperty_MACWhitelistEntriesAsValMap           "MAC:Whitelist:Entries:AsValMap"

#define kWPANTUNDProperty_JamDetectionStatus                    "JamDetection:Status"
#define kWPANTUNDProperty_JamDetectionEnable                    "JamDetection:Enable"
#define kWPANTUNDProperty_JamDetectionRssiThreshold             "JamDetection:RssiThreshold"
#define kWPANTUNDProperty_JamDetectionWindow                    "JamDetection:Window"
#define kWPANTUNDProperty_JamDetectionBusyPeriod                "JamDetection:BusyPeriod"
#define kWPANTUNDProperty_JamDetectionDebugHistoryBitmap        "JamDetection:Debug:HistoryBitmap"

#define kWPANTUNDProperty_TmfProxyEnabled                       "TmfProxy:Enabled"
#define kWPANTUNDProperty_TmfProxyStream                        "TmfProxy:Stream"

#define kWPANTUNDProperty_NestLabs_NetworkAllowingJoin          "com.nestlabs.internal:Network:AllowingJoin"
#define kWPANTUNDProperty_NestLabs_NetworkPassthruPort          "com.nestlabs.internal:Network:PassthruPort"
#define kWPANTUNDProperty_NestLabs_NCPTransmitHookActive        "com.nestlabs.internal:NCP:TransmitHookActive"
#define kWPANTUNDProperty_NestLabs_LegacyPreferInterface        "com.nestlabs.internal:Legacy:PreferInterface"
#define kWPANTUNDProperty_NestLabs_LegacyMeshLocalAddress       "com.nestlabs.internal:Legacy:MeshLocalAddress"
#define kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix        "com.nestlabs.internal:Legacy:MeshLocalPrefix"
#define kWPANTUNDProperty_NestLabs_LegacyEnabled                "com.nestlabs.internal:Legacy:Enabled"
#define kWPANTUNDProperty_NestLabs_NetworkWakeData              "com.nestlabs.internal:NetworkWake:Data"
#define kWPANTUNDProperty_NestLabs_NetworkWakeRemaining         "com.nestlabs.internal:NetworkWake:Remaining"
#define kWPANTUNDProperty_NestLabs_NetworkWakeBlacklist         "com.nestlabs.internal:NetworkWake:Blacklist"
#define kWPANTUNDProperty_NestLabs_HackUseDeepSleepOnLowPower   "com.nestlabs.internal:Hack:UseDeepSleepOnLowPower"
#define kWPANTUNDProperty_NestLabs_HackAlwaysResetToWake        "com.nestlabs.internal:Hack:AlwaysResetToWake"

#define kWPANTUNDProperty_Stat_Prefix                           "Stat:"
#define kWPANTUNDProperty_StatRX                                "Stat:RX"
#define kWPANTUNDProperty_StatTX                                "Stat:TX"
#define kWPANTUNDProperty_StatRXHistory                         "Stat:RX:History"
#define kWPANTUNDProperty_StatTXHistory                         "Stat:TX:History"
#define kWPANTUNDProperty_StatHistory                           "Stat:History"
#define kWPANTUNDProperty_StatNCP                               "Stat:NCP"
#define kWPANTUNDProperty_StatBlockingHostSleep                 "Stat:BlockingHostSleep"
#define kWPANTUNDProperty_StatNode                              "Stat:Node"
#define kWPANTUNDProperty_StatNodeHistory                       "Stat:Node:History"
#define kWPANTUNDProperty_StatNodeHistoryID                     "Stat:Node:History:"
#define kWPANTUNDProperty_StatShort                             "Stat:Short"
#define kWPANTUNDProperty_StatLong                              "Stat:Long"
#define kWPANTUNDProperty_StatAutoLog                           "Stat:AutoLog"
#define kWPANTUNDProperty_StatAutoLogState                      "Stat:AutoLog:State"
#define kWPANTUNDProperty_StatAutoLogPeriod                     "Stat:AutoLog:Period"
#define kWPANTUNDProperty_StatAutoLogLogLevel                   "Stat:AutoLog:LogLevel"
#define kWPANTUNDProperty_StatUserLogRequestLogLevel            "Stat:UserRequest:LogLevel"
#define kWPANTUNDProperty_StatLinkQuality                       "Stat:LinkQuality"
#define kWPANTUNDProperty_StatLinkQualityLong                   "Stat:LinkQuality:Long"
#define kWPANTUNDProperty_StatLinkQualityShort                  "Stat:LinkQuality:Short"
#define kWPANTUNDProperty_StatLinkQualityPeriod                 "Stat:LinkQuality:Period"
#define kWPANTUNDProperty_StatHelp                              "Stat:Help"

// ----------------------------------------------------------------------------

#define kWPANTUNDNodeType_Unknown                               "unknown"
#define kWPANTUNDNodeType_Router                                "router"
#define kWPANTUNDNodeType_EndDevice                             "end-device"
#define kWPANTUNDNodeType_SleepyEndDevice                       "sleepy-end-device"
#define kWPANTUNDNodeType_NestLurker                            "nl-lurker"
#define kWPANTUNDNodeType_Commissioner                          "commissioner"
#define kWPANTUNDNodeType_Leader                                "leader"

// ----------------------------------------------------------------------------

// When querying the value of the association state property,
// the returned value will be a human-readable string. Compare
// it with one of the constants below to get the exact meaning.
#define kWPANTUNDStateUninitialized                             "uninitialized"
#define kWPANTUNDStateFault                                     "uninitialized:fault"
#define kWPANTUNDStateUpgrading                                 "uninitialized:upgrading"
#define kWPANTUNDStateDeepSleep                                 "offline:deep-sleep"
#define kWPANTUNDStateOffline                                   "offline"
#define kWPANTUNDStateCommissioned                              "offline:commissioned"
#define kWPANTUNDStateAssociating                               "associating"
#define kWPANTUNDStateCredentialsNeeded                         "associating:credentials-needed"
#define kWPANTUNDStateAssociated                                "associated"
#define kWPANTUNDStateIsolated                                  "associated:no-parent"
#define kWPANTUNDStateNetWake_Asleep                            "associated:netwake-asleep"
#define kWPANTUNDStateNetWake_Waking                            "associated:netwake-waking"

// ----------------------------------------------------------------------------

// Values of  the property kWPANTUNDProperty_StatAutoLogState
#define kWPANTUNDStatAutoLogState_Disabled                      "disabled"
#define kWPANTUNDStatAutoLogState_Long                          "long"
#define kWPANTUNDStatAutoLogState_Short                         "short"

// ----------------------------------------------------------------------------

// Values for value map keys

#define kWPANTUNDValueMapKey_Whitelist_ExtAddress               "ExtAddress"
#define kWPANTUNDValueMapKey_Whitelist_Rssi                     "FixedRssi"

#define kWPANTUNDValueMapKey_NetworkTopology_ExtAddress         "ExtAddress"
#define kWPANTUNDValueMapKey_NetworkTopology_RLOC16             "RLOC16"
#define kWPANTUNDValueMapKey_NetworkTopology_LinkQualityIn      "LinkQualityIn"
#define kWPANTUNDValueMapKey_NetworkTopology_AverageRssi        "AverageRssi"
#define kWPANTUNDValueMapKey_NetworkTopology_LastRssi           "LastRssi"
#define kWPANTUNDValueMapKey_NetworkTopology_Age                "Age"
#define kWPANTUNDValueMapKey_NetworkTopology_RxOnWhenIdle       "RxOnWhenIdle"
#define kWPANTUNDValueMapKey_NetworkTopology_FullFunction       "FullFunction"
#define kWPANTUNDValueMapKey_NetworkTopology_SecureDataRequest  "SecureDataRequest"
#define kWPANTUNDValueMapKey_NetworkTopology_FullNetworkData    "FullNetworkData"
#define kWPANTUNDValueMapKey_NetworkTopology_Timeout            "Timeout"
#define kWPANTUNDValueMapKey_NetworkTopology_NetworkDataVersion "NetworkDataVersion"
#define kWPANTUNDValueMapKey_NetworkTopology_LinkFrameCounter   "LinkFrameCounter"
#define kWPANTUNDValueMapKey_NetworkTopology_MleFrameCounter    "MleFrameCounter"
#define kWPANTUNDValueMapKey_NetworkTopology_IsChild            "IsChild"

#define kWPANTUNDValueMapKey_Scan_Period                        "Scan:Period"
#define kWPANTUNDValueMapKey_Scan_ChannelMask                   "Scan:ChannelMask"
#define kWPANTUNDValueMapKey_Scan_Discover                      "Scan:Discover"
#define kWPANTUNDValueMapKey_Scan_JoinerFalg                    "Scan:JoinerFlag"
#define kWPANTUNDValueMapKey_Scan_EnableFiltering               "Scan:EnableFiltering"
#define kWPANTUNDValueMapKey_Scan_PANIDFilter                   "Scan:PANID"

#endif
