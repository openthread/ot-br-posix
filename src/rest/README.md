# Add REST JSON:API for commissioner and diagnostics

## Summary

In this PR, we propose an extended `REST` API functionality providing capabilities for commissioning and on-mesh diagnostics to generic off-mesh http/https clients. The implementation is guided by the [JSON:API specification](https://jsonapi.org/format/).

## Use Cases

The API targets following consumers and use cases:

- access to commissioning and network diagnostics for http/https clients with minimal code dependency (e.g. no ot-commissioner needed)
- field engineers for instant issue analysis and to visualize the mesh network topology
- continuous monitoring of network quality, eg. localisation of `weak` mesh regions
- developers for debugging and understanding network topology
- testers

A consumer may extract the data from the API and visualize it elsewhere.

## Background

- meshcop needs DTLS and Thread specific sources or binaries
- RESTful http resources following JSON:API specification eases interpretation of the large number of attributes provided and eliminates dependencies to coap and DTLS.
- enables tracking of history on-demand

## New REST JSON:API resources

### `/api/actions`

A http client may send requests to the actions resource that trigger diagnostic requests into the Thread network or requests to a (on-mesh) commissioner for onboarding new devices onto the Thread network. Such requests typically may have a longer response time and results be obtained indirectly after some processing time.

This PR supports following action requests to `/api/actions`:

- POST
  - `addThreadDeviceTask` - starts the on-mesh commissioner and adds the joiner candidate into the joiner table
- GET | DELETE the full `actions` collection

A `DELETE` request on `api/actions` cancels a ongoing action and deletes all collection items.

### Examples of general resource features

The request must comprise a header field `Accept` set to `application/vnd.api+json` for JSON:API content format, or `application/json` for JSON content format in the response. Accordingly, the response comprises the header field `Content-type`.

#### Get collection

All items of the collection are gettable in a single request, e.g.

```
curl -G http://<otbr-address>/api/<collectionName> -H 'Accept: application/vnd.api+json'
```

#### Get collection item by id

Individual items are gettable from the collections, e.g.

```
curl -G http://<otbr-address>/api/<collectionName>/<itemId> -H 'Accept: application/vnd.api+json'
```

#### Get items by type

The collection can be filtered by type of the items, e.g.

```
curl -G http://<otbr-address>/api/<collectionName>?fields[<type>] -H 'Accept: application/vnd.api+json'
```

#### Get items by type with limited attributes

The collection can be filtered by type of the items, e.g.

```
curl -G http://<otbr-address>/api/<collectionName>?fields[<type>]=attribute1,attribute2 -H 'Accept: application/vnd.api+json'
```

Detailed request examples are provided in the test folder as a "Bruno Request Collection". Use the free opensource API client [Bruno](https://www.usebruno.com/).

## Key File Description

- `actions_list.cpp`, `actions_list.hpp`

  Implements the action collection, derived from `BasicCollection`

- `add_thread_device.cpp`, `add_thread_device.hpp`

  Implements the action item stored in action collection. The action item is derived from `BasicActions` class, which is derived from `BasicCollectionItem`.

- `rest_generic_collection.cpp`, `rest_generic_collection.hpp`

  Implements the `BasicCollectionItem` and `BasicCollection` classes.

- `uuid.cpp`,`uuid.hpp`

  Implements functionality to handle UUID (generation,parse,unparse) used for collection items.

- `tests/restjsonapi/*`

  Test setup including implementation for various test cases.

# How to rebuild and experience this work

Please follow the following steps to install/build OTBR.

1. Checkout this PR

2. Build and Install OTBR as usual, e.g. on a Raspberry Pi

3. Restart the OTBR. `sudo systemctl restart otbr-agent`

4. To monitor the log [Errors|Warnings|Info] please open a different terminal instance and use following command:

```
tail -f /var/log/syslog | grep otbr
```

5. Send POST request using BRUNO or CURL, e.g. to join a new device into your network.

```
curl -X POST -H 'Content-Type: application/vnd.api+json' http://localhost:8081/api/actions -d '{"data": [{"type": "addThreadDeviceTask", "attributes": {"eui": "6234567890AACDEA", "pskd": "J01NME", "timeout": 3600}}]}' | jq
```

should return

```
{
  "data": [
    {
      "id": "2d5a8844-b1bc-4f02-93f0-d87b8c3b4e92",
      "type": "addThreadDeviceTask",
      "attributes": {
        "eui": "6234567890AACDEB",
        "pskd": "J01NME",
        "timeout": 3600,
        "status": "pending"
      },
    }
  ]
}
```

6. You may check the status and get the full collection of actions.

```
curl -X GET -H 'Accept: application/vnd.api+json' http://localhost:8081/api/actions | jq
```

should return

```
{
  "data": [
    {
      "id": "2d5a8844-b1bc-4f02-93f0-d87b8c3b4e92",
      "type": "addThreadDeviceTask",
      "attributes": {
        "eui": "6234567890AACDEB",
        "pskd": "J01NME",
        "timeout": 3600,
        "status": "pending"
      }
    }
  ],
  "meta": {
    "collection": {
      "offset": 0,
      "limit": 100,
      "total": 1
    }
  }
}
```

7. View the entry added to the commissioner's table `sudo ot-ctl commissioner joiner table` and expect

```
| ID                    | PSKd                             | Expiration |
+-----------------------+----------------------------------+------------+
|      6234567890aacdea |                           J01NME |    3459027 |
Done
```

8. Start your joiner and after a few seconds repeat above steps 6. and 7.

9. For further experiencing the diagnostic endpoints, see the python demo test script `http_action_client_demo.py` or use the Bruno request collection, you find both in the folder [tests/restjsonapi](./../../../tests/restjsonapi).

10. For running the included test script install Bruno-Cli and run the bash script on your border router

```
cd tests/restjsonapi
source ./install_bruno_cli
./test-restjsonapi-server
```

## Glossary of Names

### Diagnostic Core Attributes

Below table is derrived from Thread Specification. The column `Short Name` defines the attribute names as used in this PR.

| TLV Type | Name | Short Name (as used for JSON attributes) | TLV Length, Format of Value and Description | Can Reset? |
| --- | --- | --- | --- | --- |
| 0 | MAC Extended Address<br>(64-bit) | extAddress | Same format and semantics as the MAC Extended Address TLV (Network Layer TLV Type 1) in Section 5.19.2, MAC Extended Address TLV in Chapter 5, Network Layer. | N |
| 1 | MAC Address (16-bit) | rloc16 | Same format and semantics as the Address16 TLV (MLE TLV Type 10) in Section 4.4.9, Address16 TLV in Chapter 4, Mesh Link Establishment. | N |
| 2 | Mode (Capability Information) | mode | Same format and semantics as the Mode TLV (MLE TLV Type 1) in Section 4.4.2, Mode TLV in Chapter 4, Mesh Link Establishment. | N |
| 3 | Timeout | timeout | End Devices MUST report their MLE timeout (as described in Section 4.4.3, Timeout TLV in Chapter 4, Mesh Link Establishment) in this TLV. The same format as the MLE Timeout TLV is used. Routers MUST omit this TLV in any Diagnostic Response or Answer message. | N |
| 4 | Connectivity | connectivity | Same format and semantics as the Connectivity TLV (MLE TLV Type 15) in Section 4.4.14, Connectivity TLV in Chapter 4, Mesh Link Establishment. | N |
| 5 | Route64 | route | Same format and semantics as the Route64 TLV (MLE TLV Type 9) in Section 4.4.8, Route64 TLV in Chapter 4, Mesh Link Establishment. | N |
| 6 | Leader Data | leaderData | Same format and semantics as the Leader Data TLV (MLE TLV Type 11) in Section 4.4.10, Leader Data TLV in Chapter 4, Mesh Link Establishment. | N |
| 7 | Network Data | networkData | Same format and semantics as the Network Data TLV (MLE TLV Type 12), in Section 4.4.11, Network Data TLV in Chapter 4, Mesh Link Establishment. | N |
| 8 | IPv6 address list | ipv6Addresses | List of all link-local and higher scoped IPv6 (unicast) addresses registered by the Thread Device on this Thread Interface. With N addresses in the list the length is N\*16 bytes. All 16-byte addresses are concatenated. | N |
| 9 | MAC Counters | macCounters | TLV that contains packet/event counters for the MAC 802.15.4 interface. Defined in Section 10.11.4.1, MAC Counters TLV (9). | Y |
| 14 | Battery Level | batteryLevel | Indication of remaining battery energy. Defined in Section 10.11.4.2, Battery Level TLV (14). | N |
| 15 | Supply Voltage | supplyVoltage | Indication of the current supply voltage. Defined in Section 10.11.4.3, Supply Voltage TLV (15). | N |
| 16 | Child Table | childTable | List of all Children of a Router. Defined in Section 10.11.4.4, Child Table TLV (16). | N |
| 17 | Channel Pages | channelPages | List of supported frequency bands. Defined in Section 10.11.4.5, Channel Pages TLV (17). | N |
| 18 | Type List | - | List of type identifiers used to request or reset multiple diagnostic values. Defined in Section 10.11.4.6, Type List TLV (18). | N |
| 19 | Max Child Timeout | maxChildTimeout | Reports the maximum timeout value over a Router’s MTD Children. Defined in Section 10.11.4.7, Max Child Timeout TLV (19). | N |
| 20 | LDevID Subject Public Key Info | lDevIdSubject | The identity of the LDevID (operational) certificate of a CCM Thread Device, encoded as Subject Public Key Info. Defined in Section 10.11.4.8, LDevID Subject Public Key Info TLV (20). | N |
| 21 | IDevID Certificate | lDevIdCert | The IDevID (manufacturer) certificate of a CCM Thread Device, encoded in X.509 format. Defined in Section 10.11.4.9, IDevID Certificate TLV (21). | N |
| 22 | (reserved) | - | (reserved) | N |
| 23 | EUI-64 | eui64 | The EUI-64 of a Thread Device, binary encoded with Length = 8. | N |
| 24 | Version | version | Same format and semantics as the Version TLV (MLE TLV Type 18) in Section 4.4.17, Version TLV in Chapter 4, Mesh Link Establishment. | N |
| 25 | Vendor Name | vendorName | Same format and semantics as the Vendor Name TLV (TMF Provisioning/Discovery TLV Type 33) in Section 8.10.3.2, Vendor Name TLV in Chapter 8, Mesh Commissioning Protocol. | N |
| 26 | Vendor Model | vendorModel | Same format and semantics as the Vendor Model TLV (TMF Provisioning/Discovery TLV Type 34) in Section 8.10.3.3, Vendor Model TLV in Chapter 8, Mesh Commissioning Protocol. | N |
| 27 | Vendor SW Version | vendorSwVersion | Same format and semantics as the Vendor SW Version TLV (TMF Provisioning/Discovery TLV Type 35) in Section 8.10.3.4, Vendor SW Version TLV in Chapter 8, Mesh Commissioning Protocol. | N |
| 28 | Thread Stack Version | threadStackVersion | Thread stack version identifier as UTF-8 string. The maximum length is 64 bytes. This identifies the particular Thread stack codebase/commit/version, independent from the vendor (application) software version. | N |
| 29 | Child<sup>1</sup> | children | List of Child Containers. Each Container with diagnostic information about a particular Child of a Router. Defined in Section 10.11.4.10, Child TLV (29). | N |
| 30 | Child IPv6 Address List | childIpv6Addresses | Contains a list of IPv6 addresses of an MTD Child. Defined in Section 10.11.4.11, Child IPv6 Address List TLV (30). | N |
| 31 | Router Neighbor | routerNeighbor | Container with diagnostic information about a particular Router-neighbor of a Full Thread Device. Defined in Section 10.11.4.12, Router Neighbor TLV (31). | N |
| 32 | Answer | - | Identifies a partial answer to a diagnostic Query. Defined in Section 10.11.4.13, Answer TLV (32). | N |
| 33 | Query ID | - | Identifies a diagnostic Query, and subsequent Answer(s) to this query. Defined in Section 10.11.4.14, Query ID TLV (33). | N |
| 34 | MLE Counters | mleCounters | Contains MLE protocol related counters and timers. Defined in Section 10.11.4.15, MLE Counters TLV (34). | Y |

#### Attribute names

| Name | Short Name | Minimal supported Thread Version / DeviceType | Related TLV type |
| --- | --- | --- | --- |
| Active Routers in Partition | activeRouterCount | v1.1 | 4 |
| Age | age | FTD, v1.3.1 | 29 |
| Attach Attempts Counter | attachAttemptsCount | v1.3.1 | 34 |
| Average RSSI | avgRssi | FTD, v1.3.1 | 29, 31 |
| Battery Level | batteryLevel | v1.1 | 14 |
| Better-Partition Attach Attempts Counter | betterPartIdAttachAttemptsCount | v1.3.1 | 34 |
| C | isCslSychronized | FTD, v1.3.1 | 29 |
| Child Role Counter | childRoleCount | v1.3.1 | 34 |
| Child Role Time | childRoleTime | v1.3.1 | 34 |
| Child Timeout | childTimeout | FTD, v1.1 | 16 |
| Connection Time | linkAge | FTD, v1.3.1 | 29, 31 |
| CSL Channel | cslChannel | FTD, v1.3.1 | 29 |
| CSL Period | cslPeriod | FTD, v1.3.1 | 29 |
| CSL Timeout | cslTimeout | FTD, v1.3.1 | 29 |
| D | isFTD | FTD, v1.3.1 | 29 |
| Detached Role Counter | detachedRoleCount | v1.3.1 | 34 |
| Detached Role Time | detachedRoleTime | v1.3.1 | 34 |
| E | errorTracking | FTD, v1.3.1 | 29, 31 |
| EUI64 | eui64 | v1.3.1 | 23 |
| Extended Address | extAddress | FTD, v1.3.1 | 0, 29, 31 |
| Frame Error Rate | frameErrorRate | FTD, v1.3.1 | 29, 31 |
| ILQ | inLQ | FTD, v1.3.1 | 16 |
| Inbound Broadcast Packet Counter | ifInBroadcastPkts | v1.1 | 9 |
| Inbound Packet Discarded Counter | ifInDiscards | v1.1 | 9 |
| Inbound Packet Error Counter | ifInErrors | v1.1 | 9 |
| Inbound Packet of Unknown Protocol Counter | ifInUnknownProtos | v1.1 | 9 |
| Inbound Unicast Packet Counter | ifInUcastPkts | v1.1 | 9 |
| Ipv6 Address(es) | ipv6Addresses | FTD, v1.1, FTD, v1.3.1 | 8, 30 |
| Last RSSI | lastRssi | FTD, v1.3.1 | 29, 31 |
| Leader Cost | leaderCost | v1.1 | 4 |
| Leader Role Counter | leaderRoleCount | v1.3.1 | 34 |
| Leader Role Time | leaderRoleTime | v1.3.1 | 34 |
| Link Margin | linkMargin | FTD, v1.3.1 | 29, 31 |
| Link Quality 1 Count | linkQuality1 | v1.1 | 4 |
| Link Quality 2 Count | linkQuality2 | v1.1 | 4 |
| Link Quality 3 Count | linkQuality3 | v1.1 | 4 |
| Message Error Rate | messageErrorRate | FTD, v1.3.1 | 29, 31 |
| N | hasNetworkData | FTD, v1.3.1 | 29 |
| New Parent Counter | newParentCount | v1.3.1 | 34 |
| Outbound Broadcast Packet Counter | ifOutBroadcastPkts | v1.1 | 9 |
| Outbound Packet Discarded Counter | ifOutDiscards | v1.1 | 9 |
| Outbound Packet Error Counter | ifOutErrors | v1.1 | 9 |
| Outbound Unicast Packet Counter | ifOutUcastPkts | v1.1 | 9 |
| Partition ID Changes Counter | partIdChangesCount | v1.3.1 | 34 |
| Queued Message Count | queuedMessageCount | FTD, v1.3.1 | 29 |
| R | rxOnWhenIdle | FTD, v1.3.1 | 29 |
| Radio Disabled Counter | radioDisabledCount | v1.3.1 | 34 |
| Radio Disabled Time | radioDisabledTime | v1.3.1 | 34 |
| RLOC16 | rloc16 | FTD, v1.3.1 | 1, 29, 30, 31 |
| Router Role Counter | routerRoleCount | v1.3.1 | 34 |
| Router Role Time | routerRoleTime | v1.3.1 | 34 |
| Rx-off Child Buffer Size | rxOffChildBufferSize | v1.1 | 4 |
| Rx-off Child Datagram Count | rxOffChildDatagramCount | v1.1 | 4 |
| Supervision Interval | supervisionInterval | FTD, v1.3.1 | 29 |
| Thread Extended PanId | extPanId | v1.1 | - |
| Thread Network Name | networkName | v1.1 | - |
| Thread PanId | panId | v1.1 | - |
| Thread Stack Version | threadStackVersion | v1.3.1 | 28 |
| Thread Version | threadVersion | FTD, v1.3.1 | 29, 31 |
| Thread Version | threadVersion | v1.3.1 | 24 |
| Total Tracking Time | totalTrackingTime | v1.3.1 | 34 |
| Vendor Model | vendorModel | v1.3.1 | 26 |
| Vendor Name | vendorName | v1.3.1 | 25 |
| Vendor SW Version | vendorSwVersion | v1.3.1 | 27 |
