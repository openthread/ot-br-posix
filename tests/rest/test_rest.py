#!/usr/bin/env python3
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

import urllib.request
import urllib.error
import ipaddress
import json
import re
import time
from threading import Thread

rest_api_addr = "http://0.0.0.0:8081"


def assert_is_ipv6_address(string):
    assert (type(ipaddress.ip_address(string)) is ipaddress.IPv6Address)

def get_data_from_url(url, result, index):
    response = urllib.request.urlopen(urllib.request.Request(url, headers={'Accept': 'application/json'}))
    body = response.read()
    data = json.loads(body)
    result[index] = data


def get_error_from_url(url, result, index):
    try:
        urllib.request.urlopen(urllib.request.Request(url))
        assert False

    except urllib.error.HTTPError as e:
        result[index] = e


def create_multi_thread(func, url, thread_num, response_data):
    threads = [None] * thread_num

    for i in range(thread_num):
        threads[i] = Thread(target=func, args=(url, response_data, i))

    for thread in threads:
        thread.start()

    for thread in threads:
        thread.join()


def error404_check(data):
    assert data is not None

    assert (data.code == 404)

    return True


def diagnostics_check(data):
    assert data is not None

    if len(data) == 0:
        return 1
    for diag in data:
        expected_keys = [
            "extAddress", "rloc16", "mode", "connectivity", "route",
            "leaderData", "networkData", "ip6AddressList", "macCounters",
            "childTable", "channelPages", "maxChildTimeout", "version",
            "vendorName", "vendorModel", "vendorSwVersion", "threadStackVersion"
        ]
        expected_value_type = [
            str, str, dict, dict, dict, dict, str, list, dict, list,
            str, int, int, str, str, str, str
        ]
        expected_check_dict = dict(zip(expected_keys, expected_value_type))

        for key, value in expected_check_dict.items():
            assert (key in diag)
            assert (type(diag[key]) == value)

        assert (re.match(r'^[a-f0-9]{16}$', diag["extAddress"]) is not None)

        mode = diag["mode"]
        mode_expected_keys = [
            "rxOnWhenIdle", "deviceTypeFTD", "fullNetworkData"
        ]
        for key in mode_expected_keys:
            assert (key in mode)
            assert (type(mode[key]) == bool)

        connectivity = diag["connectivity"]
        connectivity_expected_keys = [
            "parentPriority", "linkQuality3", "linkQuality2", "linkQuality1",
            "leaderCost", "idSequence", "activeRouters", "sedBufferSize",
            "sedDatagramCount"
        ]
        for key in connectivity_expected_keys:
            assert (key in connectivity)
            assert (type(connectivity[key]) == int)

        route = diag["route"]
        assert ("idSequence" in route)
        assert (type(route["idSequence"]) == int)

        assert ("routeData" in route)
        route_routedata = route["routeData"]
        assert (type(route["routeData"]) == list)

        routedata_expected_keys = [
            "routeId", "linkQualityOut", "linkQualityIn", "routeCost"
        ]

        for item in route_routedata:
            for key in routedata_expected_keys:
                assert (key in item)
                assert (type(item[key]) == int)

        leaderdata = diag["leaderData"]
        leaderdata_expected_keys = [
            "partitionId", "weighting", "dataVersion", "stableDataVersion",
            "leaderRouterId"
        ]

        for key in leaderdata_expected_keys:
            assert (key in leaderdata)
            assert (type(leaderdata[key]) == int)

        assert (re.match(r'^[a-f0-9]', diag["networkData"]) is not None)

        ip6_address_list = diag["ip6AddressList"]
        assert (type(ip6_address_list) == list)

        for ip6_address in ip6_address_list:
            assert (type(ip6_address) == str)
            assert_is_ipv6_address(ip6_address)

        mac_counters = diag["macCounters"]
        assert (type(mac_counters) == dict)
        mac_counters_expected_keys = [
            "ifInUnknownProtos", "ifInErrors", "ifOutErrors", "ifInUcastPkts",
            "ifInBroadcastPkts", "ifInDiscards", "ifOutUcastPkts",
            "ifOutBroadcastPkts", "ifOutDiscards"
        ]
        for key in mac_counters_expected_keys:
            assert (key in mac_counters)
            assert (type(mac_counters[key]) == int)

        child_table = diag["childTable"]
        assert (type(child_table) == list)

        for child in child_table:
            assert ("childId" in child)
            assert (type(child["childId"]) == int)
            assert ("timeout" in child)
            assert (type(child["timeout"]) == int)
            assert ("mode" in child)
            mode = child["mode"]
            assert (type(mode) == dict)
            for key in mode_expected_keys:
                assert (key in mode)
                assert (type(mode[key]) == int)

        assert (type(diag["channelPages"]) == str)
        assert (re.match(r'^[a-f0-9]{2}$', diag["channelPages"]) is not None)

    return 2


def node_check(data):
    assert data is not None

    expected_keys = [
        "state", "routerCount", "rlocAddress", "networkName", "extAddress",
        "rloc16", "leaderData", "extPanId"
    ]
    expected_value_type = [
        str, int, str, str, str, str, dict, str
    ]
    expected_check_dict = dict(zip(expected_keys, expected_value_type))

    for key, value in expected_check_dict.items():
        assert (key in data)
        assert (type(data[key]) == value)

    assert_is_ipv6_address(data["rlocAddress"])

    assert (re.match(r'^[a-f0-9]{16}$', data["extAddress"]) is not None)
    assert (re.match(r'[a-f0-9]{16}', data["extPanId"]) is not None)

    leaderdata = data["leaderData"]
    leaderdata_expected_keys = [
        "partitionId", "weighting", "dataVersion", "stableDataVersion",
        "leaderRouterId"
    ]

    for key in leaderdata_expected_keys:
        assert (key in leaderdata)
        assert (type(leaderdata[key]) == int)

    return True


def node_rloc_check(data):
    assert data is not None

    assert (type(data) == str)

    assert_is_ipv6_address(data)

    return True


def node_rloc16_check(data):
    assert data is not None

    assert (type(data) == int)

    return True


def node_ext_address_check(data):
    assert data is not None

    assert (type(data) == str)
    assert (re.match(r'^[a-f0-9]{16}$', data) is not None)

    return True


def node_state_check(data):
    assert data is not None

    assert (type(data) == str)

    return True

def node_state_check_attached(data):
    node_state_check(data)
        
    if (data != "detached") and (data != "disabled"):
        return True
    else:
        return False


def node_network_name_check(data):
    assert data is not None

    assert (type(data) == str)

    return True


def node_leader_data_check(data):
    assert data is not None

    assert (type(data) == dict)

    leaderdata_expected_keys = [
        "partitionId", "weighting", "dataVersion", "stableDataVersion",
        "leaderRouterId"
    ]

    for key in leaderdata_expected_keys:
        assert (key in data)
        assert (type(data[key]) == int)

    return True


def node_num_of_router_check(data):
    assert data is not None

    assert (type(data) == int)

    return True


def node_ext_panid_check(data):
    assert data is not None

    assert (type(data) == str)

    return True


def node_coprocessor_version_check(data):
    assert data is not None

    assert (type(data) == str)

    return True


def node_test(thread_num):
    url = rest_api_addr + "/node"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_check(data) for data in response_data].count(True)

    print(" /node : all {}, valid {} ".format(thread_num, valid))


def node_rloc_test(thread_num):
    url = rest_api_addr + "/node/rloc"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_rloc_check(data) for data in response_data].count(True)

    print(" /node/rloc : all {}, valid {} ".format(thread_num, valid))


def node_rloc16_test(thread_num):
    url = rest_api_addr + "/node/rloc16"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_rloc16_check(data) for data in response_data].count(True)

    print(" /node/rloc16 : all {}, valid {} ".format(thread_num, valid))


def node_ext_address_test(thread_num):
    url = rest_api_addr + "/node/ext-address"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_ext_address_check(data) for data in response_data].count(True)

    print(" /node/ext-address : all {}, valid {} ".format(thread_num, valid))


def node_state_test(thread_num):
    url = rest_api_addr + "/node/state"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_state_check(data) for data in response_data].count(True)

    print(" /node/state : all {}, valid {} ".format(thread_num, valid))


def node_state_test_attached():
    url = rest_api_addr + "/node/state"

    response_data = [None]
    is_attached = False
    now = time.time()

    while not is_attached and time.time() - now < 130:
        get_data_from_url(url, response_data, 0)
        is_attached = node_state_check_attached(response_data[0])
        print(" /node/state : attached {} ".format(is_attached))
        if not is_attached:
            time.sleep(1)


def node_network_name_test(thread_num):
    url = rest_api_addr + "/node/network-name"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_network_name_check(data) for data in response_data
             ].count(True)

    print(" /node/network-name : all {}, valid {} ".format(thread_num, valid))


def node_leader_data_test(thread_num):
    url = rest_api_addr + "/node/leader-data"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_leader_data_check(data) for data in response_data].count(True)

    print(" /node/leader-data : all {}, valid {} ".format(thread_num, valid))


def node_num_of_router_test(thread_num):
    url = rest_api_addr + "/node/num-of-router"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_num_of_router_check(data) for data in response_data
             ].count(True)

    print(" /v1/node/num-of-router : all {}, valid {} ".format(thread_num, valid))


def node_ext_panid_test(thread_num):
    url = rest_api_addr + "/node/ext-panid"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_ext_panid_check(data) for data in response_data].count(True)

    print(" /node/ext-panid : all {}, valid {} ".format(thread_num, valid))


def node_coprocessor_version_test(thread_num):
    url = rest_api_addr + "/node/coprocessor/version"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_coprocessor_version_check(data) for data in response_data].count(True)

    print(" /node/coprocessor/version : all {}, valid {} ".format(thread_num, valid))


def diagnostics_test(thread_num):
    url = rest_api_addr + "/diagnostics"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

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

    create_multi_thread(get_error_from_url, url, thread_num, response_data)

    valid = [error404_check(data) for data in response_data].count(True)

    print(" /v1/hello : all {}, valid {} ".format(thread_num, valid))


def main():
    node_test(200)
    node_rloc_test(200)
    node_rloc16_test(200)
    node_ext_address_test(200)
    node_state_test(200)
    node_network_name_test(200)
    node_state_test_attached()  # wait for attached state
    node_leader_data_test(200)
    node_num_of_router_test(200)
    node_ext_panid_test(200)
    node_coprocessor_version_test(200)
    # diagnostics_test(20)  # partly replaced with restjsonapi tests
    error_test(10)

    return 0


if __name__ == '__main__':
    exit(main())
