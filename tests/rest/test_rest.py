#!/usr/bin/python3
#
#  Copyright (c) 2020, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

import socket
import sys
import time
import urllib2
import json
import re
from threading import Thread

rest_api_addr = "http://localhost:81"


def get_data_from_url(url, result, index):

    try:

        response = urllib2.urlopen(urllib2.Request(url))
        time_stamp = time.time()
        body = response.read()
        data = json.loads(body)
        result[index] = data

    except:
        result[index] = None


def create_multithread(url, thread_num, response_data):
    threads = [None] * thread_num

    for i in range(thread_num):
        threads[i] = Thread(target=get_data_from_url,
                            args=(url, response_data, i))

    for thread in threads:
        thread.start()

    for thread in threads:
        thread.join()


def diagnostics_check(data):

    if data == None:
        return 0
    if len(data) == 0:
        return 1
    for diag in data:
        expected_keys = [
            "ExtAddress", "Rloc16", "Mode", "Connectivity", "Route",
            "LeaderData", "NetworkData", "IP6AddressList", "MACCounters",
            "ChildTable", "ChannelPages"
        ]
        expected_value_type = [
            unicode, int, dict, dict, dict, dict, unicode, list, dict, list,
            unicode
        ]
        expected_check_dict = dict(zip(expected_keys, expected_value_type))

        for key, value in expected_check_dict.items():
            assert (diag.has_key(key))
            assert (type(diag[key]) == value)

        assert (None != re.match(ur'^[A-F0-9]{16}$', diag["ExtAddress"]))

        mode = diag["Mode"]
        mode_expected_keys = [
            "RxOnWhenIdle", "SecureDataRequests", "DeviceType", "NetworkData"
        ]
        for key in mode_expected_keys:
            assert (mode.has_key(key))
            assert (type(mode[key]) == int)

        connectivity = diag["Connectivity"]
        connectivity_expected_keys = [
            "ParentPriority", "LinkQuality3", "LinkQuality2", "LinkQuality1",
            "LeaderCost", "IdSequence", "ActiveRouters", "SedBufferSize",
            "SedDatagramCount"
        ]
        for key in connectivity_expected_keys:
            assert (connectivity.has_key(key))
            assert (type(connectivity[key]) == int)

        route = diag["Route"]
        assert (route.has_key("IdSequence"))
        assert (type(route["IdSequence"]) == int)

        assert (route.has_key("RouteData"))
        route_routedata = route["RouteData"]
        assert (type(route["RouteData"]) == list)

        routedata_expected_keys = [
            "RouteId", "LinkQualityOut", "LinkQualityIn", "RouteCost"
        ]

        for item in route_routedata:
            for key in routedata_expected_keys:
                assert (item.has_key(key))
                assert (type(item[key]) == int)

        leaderdata = diag["LeaderData"]
        leaderdata_expected_keys = [
            "PartitionId", "Weighting", "DataVersion", "StableDataVersion",
            "LeaderRouterId"
        ]

        for key in leaderdata_expected_keys:
            assert (leaderdata.has_key(key))
            assert (type(leaderdata[key]) == int)

        assert (None != re.match(ur'^[A-F0-9]{12}$', diag["NetworkData"]))

        ip6_address_list = diag["IP6AddressList"]
        assert (type(ip6_address_list) == list)

        for ip6_address in ip6_address_list:
            assert (type(ip6_address) == unicode)
            assert (None != re.match(
                ur'^[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+$',
                ip6_address))

        mac_counters = diag["MACCounters"]
        assert (type(mac_counters) == dict)
        mac_counters_expected_keys = [
            "IfInUnknownProtos", "IfInErrors", "IfOutErrors", "IfInUcastPkts",
            "IfInBroadcastPkts", "IfInDiscards", "IfOutUcastPkts",
            "IfOutBroadcastPkts", "IfOutDiscards"
        ]
        for key in mac_counters_expected_keys:
            assert (mac_counters.has_key(key))
            assert (type(mac_counters[key]) == int)

        child_table = diag["ChildTable"]
        assert (type(child_table) == list)

        for child in child_table:
            assert (child.has_key("ChildId"))
            assert (type(child["ChildId"]) == int)
            assert (child.has_key("Timeout"))
            assert (type(child["Timeout"]) == int)
            assert (child.has_key("Mode"))
            mode = child["Mode"]
            assert (type(mode) == dict)
            for key in mode_expected_keys:
                assert (mode.has_key(key))
                assert (type(mode[key]) == int)

        assert (type(diag["ChannelPages"]) == unicode)
        assert (None != re.match(ur'^[A-F0-9]{2}$', diag["ChannelPages"]))

    return 2


def node_check(data):
    if data == None:
        return False

    expected_keys = [
        "State", "NumOfRouter", "RlocAddress", "NetworkName", "ExtAddress",
        "Rloc16", "LeaderData", "ExtPanId"
    ]
    expected_value_type = [
        int, int, unicode, unicode, unicode, int, dict, unicode
    ]
    expected_check_dict = dict(zip(expected_keys, expected_value_type))

    for key, value in expected_check_dict.items():
        assert (data.has_key(key))
        assert (type(data[key]) == value)

    assert (None != re.match(
        ur'^[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+$',
        data["RlocAddress"]))
    assert (None != re.match(ur'^[A-F0-9]{16}$', data["ExtAddress"]))
    assert (None != re.match(ur'[A-F0-9]{16}', data["ExtPanId"]))

    leaderdata = data["LeaderData"]
    leaderdata_expected_keys = [
        "PartitionId", "Weighting", "DataVersion", "StableDataVersion",
        "LeaderRouterId"
    ]

    for key in leaderdata_expected_keys:
        assert (leaderdata.has_key(key))
        assert (type(leaderdata[key]) == int)

    return True


def node_rloc_check(data):

    if data == None:
        return False

    assert (type(data) == unicode)

    assert (None != re.match(
        ur'^[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+$',
        data))

    return True


def node_rloc16_check(data):

    if data == None:
        return False

    assert (type(data) == int)

    return True


def node_ext_address_check(data):

    if data == None:
        return False

    assert (type(data) == unicode)
    assert (None != re.match(ur'^[A-F0-9]{16}$', data))

    return True


def node_state_check(data):

    if data == None:
        return False

    assert (type(data) == int)

    return True


def node_network_name_check(data):

    if data == None:
        return False

    assert (type(data) == unicode)

    return True


def node_leader_data_check(data):

    if data == None:
        return False

    assert (type(data) == dict)

    leaderdata_expected_keys = [
        "PartitionId", "Weighting", "DataVersion", "StableDataVersion",
        "LeaderRouterId"
    ]

    for key in leaderdata_expected_keys:
        assert (data.has_key(key))
        assert (type(data[key]) == int)

    return True


def node_num_of_router_check(data):

    if data == None:
        return False

    assert (type(data) == int)

    return True


def node_ext_panid_check(data):

    if data == None:
        return False

    assert (type(data) == unicode)

    return True


def error_check(data):

    if data == None:
        return False

    assert (data == u'/hello')

    return True


def node_test(thread_num):
    url = rest_api_addr + "/node"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_check(data) for data in response_data].count(True)

    print(" /node : all {}, valid {} ".format(thread_num, valid))


def node_rloc_test(thread_num):

    url = rest_api_addr + "/node/rloc"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_rloc_check(data) for data in response_data].count(True)

    print(" /node/rloc : all {}, valid {} ".format(thread_num, valid))


def node_rloc16_test(thread_num):

    url = rest_api_addr + "/node/rloc16"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_rloc16_check(data) for data in response_data].count(True)

    print(" /node/rloc16 : all {}, valid {} ".format(thread_num, valid))


def node_ext_address_test(thread_num):

    url = rest_api_addr + "/node/ext-address"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_ext_address_check(data) for data in response_data].count(True)

    print(" /node/ext-address : all {}, valid {} ".format(thread_num, valid))


def node_state_test(thread_num):

    url = rest_api_addr + "/node/state"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_state_check(data) for data in response_data].count(True)

    print(" /node/state : all {}, valid {} ".format(thread_num, valid))


def node_network_name_test(thread_num):

    url = rest_api_addr + "/node/network-name"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_network_name_check(data) for data in response_data
            ].count(True)

    print(" /node/network-name : all {}, valid {} ".format(thread_num, valid))


def node_leader_data_test(thread_num):

    url = rest_api_addr + "/node/leader-data"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_leader_data_check(data) for data in response_data].count(True)

    print(" /node/leader-data : all {}, valid {} ".format(thread_num, valid))


def node_num_of_router_test(thread_num):

    url = rest_api_addr + "/node/num-of-router"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_num_of_router_check(data) for data in response_data
            ].count(True)

    print(" /node/num-of-router : all {}, valid {} ".format(thread_num, valid))


def node_ext_panid_test(thread_num):

    url = rest_api_addr + "/node/ext-panid"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [node_ext_panid_check(data) for data in response_data].count(True)

    print(" /node/ext-panid : all {}, valid {} ".format(thread_num, valid))


def diagnostics_test(thread_num):

    url = rest_api_addr + "/diagnostics"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = 0
    has_content = 0
    for data in response_data:
        ret = diagnostics_check(data)
        if ret == 1:
            valid += 1
        elif ret == 2:
            valid += 1
            has_content += 1

    print(" /diagnostics : all {}, has content {}, valid {} ".format(
        thread_num, has_content, valid))


def error_test(thread_num):

    url = rest_api_addr + "/hello"

    response_data = [None] * thread_num

    create_multithread(url, thread_num, response_data)

    valid = [error_check(data) for data in response_data].count(True)

    print(" /hello : all {}, valid {} ".format(thread_num, valid))


def main():

    node_test(200)
    node_rloc_test(200)
    node_rloc16_test(200)
    node_ext_address_test(200)
    node_state_test(200)
    node_network_name_test(200)
    node_leader_data_test(200)
    node_num_of_router_test(200)
    node_ext_panid_test(200)
    diagnostics_test(200)
    error_test(10)

    return 0


if __name__ == '__main__':
    exit(main())
