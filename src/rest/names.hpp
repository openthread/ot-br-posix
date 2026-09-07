/*
 *  Copyright (c) 2026, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes JSON key names for RESTful HTTP server.
 *
 *   Sorted alphabetically by key name.
 */

#ifndef OTBR_REST_NAMES_HPP_
#define OTBR_REST_NAMES_HPP_

#define ADD_DEVICE_ACTION_TYPE_NAME "addThreadDeviceTask"
#define DISCOVER_NETWORK_ACTION_TYPE_NAME "updateDeviceCollectionTask"
#define ENERGY_SCAN_ACTION_TYPE_NAME "getEnergyScanTask"
#define KEY_ACTIVEDATASET "activeDataset"
#define KEY_ACTIVEROUTERS "activeRouters"
#define KEY_ACTIVETIMESTAMP "activeTimestamp"
#define KEY_AGE "age"
#define KEY_ATTACHATTEMPTSCOUNT "attachAttemptsCount"
#define KEY_ATTRIBUTES "attributes"
#define KEY_AUTHORITATIVE "authoritative"
#define KEY_AUTONOMOUSENROLLMENT "autonomousEnrollment"
#define KEY_AVERAGERSSI "averageRssi"
#define KEY_BATTERYLEVEL "batteryLevel" // Battery energy level
#define KEY_BETTERPARTIDATTEMPTSCOUNT "betterPartIdAttachAttemptsCount"
#define KEY_BORDERAGENTID "baId"
#define KEY_BORDERAGENTSTATE "baState"
#define KEY_BRCOUNTERS "brCounters"
#define KEY_CHANNEL "channel"
#define KEY_CHANNELMASK "channelMask"
#define KEY_CHANNELPAGES "channelPages" // Supported frequency bands
#define KEY_CHILDID "childId"
#define KEY_CHILDIPV6ADDRESSES "childIpv6Addresses" // IPv6 addresses of child
#define KEY_CHILDREN "children"                     // Child TLV 29
#define KEY_CHILDROLECOUNT "childRoleCount"
#define KEY_CHILDROLETIME "childRoleTime"
#define KEY_CHILDTABLE "childTable" // List of children
#define KEY_COLLECTION "collection"
#define KEY_COMMERCIALCOMMISSIONING "commercialCommissioning"
#define KEY_CONNECTIVITY "connectivity"
#define KEY_COUNT "count"
#define KEY_CSLCHANNEL "cslChannel"
#define KEY_CSLPERIOD "cslPeriod"
#define KEY_CSLSYNCHRONIZED "cslSynchronized"
#define KEY_CSLTIMEOUT "cslTimeout"
#define KEY_DATA "data"
#define KEY_DATAVERSION "dataVersion"
#define KEY_DELAY "delay"
#define KEY_DESTINATION "destination"
#define KEY_DESTINATION_TYPE "destinationType"
#define KEY_DETACHEDROLECOUNT "detachedRoleCount"
#define KEY_DETACHEDROLETIME "detachedRoleTime"
#define KEY_DETAIL "detail"
#define KEY_DEVICE_COUNT "deviceCount"
#define KEY_DISCERNER "discerner"
#define KEY_EUI "eui"               // EUI-64 address
#define KEY_EXTADDRESS "extAddress" // 64-bit MAC address
#define KEY_EXTERNALCOMMISSIONING "externalCommissioning"
#define KEY_EXTPANID "extPanId"
#define KEY_FRAMEERRORRATE "frameErrorRate"
#define KEY_FULLNETWORKDATA "fullNetworkData"
#define KEY_HOSTNAME "hostname"
#define KEY_ID "id"
#define KEY_IDEVIDCERT "iDevIdCert" // IDevID certificate
#define KEY_IDSEQUENCE "idSequence"
#define KEY_IFINBROADCASTPKTS "ifInBroadcastPkts"
#define KEY_IFINDISCARDS "ifInDiscards"
#define KEY_IFINERRORS "ifInErrors"
#define KEY_IFINUCASTPKTS "ifInUcastPkts"
#define KEY_IFINUNKNOWNPROTOS "ifInUnknownProtos"
#define KEY_IFOUTBROADCASTPKTS "ifOutBroadcastPkts"
#define KEY_IFOUTDISCARDS "ifOutDiscards"
#define KEY_IFOUTERRORS "ifOutErrors"
#define KEY_IFOUTUCASTPKTS "ifOutUcastPkts"
#define KEY_IP6ADDRESSLIST "ipv6Addresses"
#define KEY_ISBR "isBorderRouter"
#define KEY_ISFTD "fullThreadDevice"
#define KEY_ISLEADER "isLeader"
#define KEY_ISPBBR "isPrimaryBBR"
#define KEY_JOINERID "joinerId"
#define KEY_LASTRSSI "lastRssi"
#define KEY_LDEVIDSUBJECT "lDevIdSubject" // LDevID subject public key info
#define KEY_LEADERCOST "leaderCost"
#define KEY_LEADERDATA "leaderData" // Leader data
#define KEY_LEADERROLECOUNT "leaderRoleCount"
#define KEY_LEADERROLETIME "leaderRoleTime"
#define KEY_LEADERROUTERID "leaderRouterId"
#define KEY_LIMIT "limit"
#define KEY_LINKAGE "linkAge"
#define KEY_LINKMARGIN "linkMargin"
#define KEY_LINKQUALITY "linkQuality"
#define KEY_LINKQUALITY1 "linkQuality1"
#define KEY_LINKQUALITY2 "linkQuality2"
#define KEY_LINKQUALITY3 "linkQuality3"
#define KEY_LINKQUALITYIN "linkQualityIn"
#define KEY_LINKQUALITYOUT "linkQualityOut"
#define KEY_MACCOUNTERS "macCounters"
#define KEY_MAXCHILDTIMEOUT "maxChildTimeout"
#define KEY_MAXRSSI "maxRssi"
#define KEY_MAX_AGE "maxAge"
#define KEY_MAX_RETRIES "maxRetries"
#define KEY_MESHLOCALPREFIX "meshLocalPrefix"
#define KEY_MESSAGEERRORRATE "messageErrorRate"
#define KEY_META "meta"
#define KEY_MLECOUNTERS "mleCounters"
#define KEY_MLEIDIID "mlEidIid"
#define KEY_MODE "mode"
#define KEY_NATIVECOMMISSIONING "nativeCommissioning"
#define KEY_NETWORKDATA "networkData"
#define KEY_NETWORKKEY "networkKey"
#define KEY_NETWORKKEYPROVISIONING "networkKeyProvisioning"
#define KEY_NETWORKNAME "networkName"
#define KEY_NEWPARENTCOUNT "newParentCount"
#define KEY_NONCCMROUTERS "nonCcmRouters"
#define KEY_OBTAINNETWORKKEY "obtainNetworkKey"
#define KEY_OFFSET "offset"
#define KEY_OMRIPV6 "omrIpv6Address"
#define KEY_ORIGIN "origin"
#define KEY_PANID "panId"
#define KEY_PARENTPRIORITY "parentPriority"
#define KEY_PARTIDCHANGESCOUNT "partIdChangesCount"
#define KEY_PARTITIONID "partitionId"
#define KEY_PENDING "pending"
#define KEY_PENDINGTIMESTAMP "pendingTimestamp"
#define KEY_PERIOD "period"
#define KEY_PSKC "pskc"
#define KEY_PSKD "pskd"
#define KEY_QUEUEDMESSAGECOUNT "queuedMessageCount"
#define KEY_RADIODISABLEDCOUNT "radioDisabledCount"
#define KEY_RADIODISABLEDTIME "radioDisabledTime"
#define KEY_RARX "raRx"
#define KEY_RATXFAILED "raTxFailed"
#define KEY_RATXSUCCESS "raTxSuccess"
#define KEY_RELATIONSHIPS "relationships"
#define KEY_REPORT "report"
#define KEY_RLOC16 "rloc16" // 16-bit MAC address
#define KEY_RLOC16_IPV6ADDRESS "rlocAddress"
#define KEY_ROLE "role"
#define KEY_ROTATIONTIME "rotationTime"
#define KEY_ROUTE "route" // Route64 TLV name
#define KEY_ROUTECOST "routeCost"
#define KEY_ROUTEDATA "routeData"
#define KEY_ROUTEID "routeId"
#define KEY_ROUTERCOUNT "routerCount"
#define KEY_ROUTERID "routerId"
#define KEY_ROUTERNEIGHBORS "routerNeighbors" // Router neighbor info
#define KEY_ROUTERROLECOUNT "routerRoleCount"
#define KEY_ROUTERROLETIME "routerRoleTime"
#define KEY_ROUTERS "routers"
#define KEY_RSRX "rsRx"
#define KEY_RSTXFAILED "rsTxFailed"
#define KEY_RSTXSUCCESS "rsTxSuccess"
#define KEY_RXONWHENIDLE "rxOnWhenIdle"
#define KEY_SCANDURATION "scanDuration"
#define KEY_SECONDS "seconds"
#define KEY_SECURITYPOLICY "securityPolicy"
#define KEY_SEDBUFFERSIZE "sedBufferSize"
#define KEY_SEDDATAGRAMCOUNT "sedDatagramCount"
#define KEY_SERVICE "hostsService"
#define KEY_STABLEDATAVERSION "stableDataVersion"
#define KEY_STATE "state"
#define KEY_STATUS "status"
#define KEY_SUPERVISIONINTERVAL "supervisionInterval"
#define KEY_SUPPLYVOLTAGE "supplyVoltage" // Current supply voltage
#define KEY_SUPPORTSERRORRATE "supportsErrorRate"
#define KEY_THREADSTACKVERSION "threadStackVersion"
#define KEY_THREADVERSION "threadVersion"
#define KEY_TICKS "ticks"
#define KEY_TIMEOUT "timeout"
#define KEY_TITLE "title"
#define KEY_TOBLELINK "tobleLink"
#define KEY_TOTAL "total"
#define KEY_TOTALTRACKINGTIME "totalTrackingTime"
#define KEY_TYPE "type"
#define KEY_TYPES "types"
#define KEY_VENDORMODEL "vendorModel"
#define KEY_VENDORNAME "vendorName"
#define KEY_VENDORSWVERSION "vendorSwVersion" // Vendor software version
#define KEY_WEIGHTING "weighting"
#define NETWORK_DIAG_ACTION_TYPE_NAME "getNetworkDiagnosticTask"
#define RESET_DIAG_COUNTERS_ACTION_TYPE_NAME "resetNetworkDiagCounterTask"
#endif // OTBR_REST_NAMES_HPP_
