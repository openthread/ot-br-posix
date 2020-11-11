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

import json
import re
import time
from threading import Thread
import urllib.request
import urllib.error
import urllib.parse

REST_API_ADDR = "http://0.0.0.0:8081"

def get_data_from_url(url, result, index):
    response = urllib.request.urlopen(urllib.request.Request(url))
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
            "ExtAddress", "Rloc16", "Mode", "Connectivity", "Route",
            "LeaderData", "NetworkData", "IP6AddressList", "MACCounters",
            "ChildTable", "ChannelPages"
        ]
        expected_value_type = [
            str, int, dict, dict, dict, dict, str, list, dict, list, str
        ]
        expected_check_dict = dict(zip(expected_keys, expected_value_type))

        for key, value in expected_check_dict.items():
            assert (key in diag)
            assert (type(diag[key]) == value)

        assert (re.match(r'^[a-f0-9]{16}$', diag["ExtAddress"]) is not None)

        mode = diag["Mode"]
        mode_expected_keys = [
            "RxOnWhenIdle", "DeviceType", "NetworkData"
        ]
        for key in mode_expected_keys:
            assert (key in mode)
            assert (type(mode[key]) == int)

        connectivity = diag["Connectivity"]
        connectivity_expected_keys = [
            "ParentPriority", "LinkQuality3", "LinkQuality2", "LinkQuality1",
            "LeaderCost", "IdSequence", "ActiveRouters", "SedBufferSize",
            "SedDatagramCount"
        ]
        for key in connectivity_expected_keys:
            assert (key in connectivity)
            assert (type(connectivity[key]) == int)

        route = diag["Route"]
        assert ("IdSequence" in route)
        assert (type(route["IdSequence"]) == int)

        assert ("RouteData" in route)
        route_routedata = route["RouteData"]
        assert (type(route["RouteData"]) == list)

        routedata_expected_keys = [
            "RouteId", "LinkQualityOut", "LinkQualityIn", "RouteCost"
        ]

        for item in route_routedata:
            for key in routedata_expected_keys:
                assert (key in item)
                assert (type(item[key]) == int)

        leaderdata = diag["LeaderData"]
        leaderdata_expected_keys = [
            "PartitionId", "Weighting", "DataVersion", "StableDataVersion",
            "LeaderRouterId"
        ]

        for key in leaderdata_expected_keys:
            assert (key in leaderdata)
            assert (type(leaderdata[key]) == int)

        ip6_address_list = diag["IP6AddressList"]
        assert (type(ip6_address_list) == list)

        for ip6_address in ip6_address_list:
            assert (type(ip6_address) == str)
            assert (re.match(
                r'^[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+$',
                ip6_address) is not None)

        mac_counters = diag["MACCounters"]
        assert (type(mac_counters) == dict)
        mac_counters_expected_keys = [
            "IfInUnknownProtos", "IfInErrors", "IfOutErrors", "IfInUcastPkts",
            "IfInBroadcastPkts", "IfInDiscards", "IfOutUcastPkts",
            "IfOutBroadcastPkts", "IfOutDiscards"
        ]
        for key in mac_counters_expected_keys:
            assert (key in mac_counters)
            assert (type(mac_counters[key]) == int)

        child_table = diag["ChildTable"]
        assert (type(child_table) == list)

        for child in child_table:
            assert ("ChildId" in child)
            assert (type(child["ChildId"]) == int)
            assert ("Timeout" in child)
            assert (type(child["Timeout"]) == int)
            assert ("Mode" in child)
            mode = child["Mode"]
            assert (type(mode) == dict)
            for key in mode_expected_keys:
                assert (key in mode)
                assert (type(mode[key]) == int)

        assert (type(diag["ChannelPages"]) == str)
        assert (re.match(r'^[a-f0-9]{2}$', diag["ChannelPages"]) is not None)

    return 2


def node_check(data):
    assert data is not None

    expected_keys = [
        "WPAN service", "NCP:State", "NCP:Version", "NCP:HardwareAddress",
        "NCP:Channel", "Network:NodeType", "Network:Name", "Network:XPANID",
        "Network:PANID", "IPv6:MeshLocalPrefix", "IPv6:MeshLocalAddress"
    ]
    expected_value_type = [
        str, int, str, str, int, int, str, str, int, str, str
    ]
    expected_check_dict = dict(zip(expected_keys, expected_value_type))

    for key, value in expected_check_dict.items():
        assert (key in data)
        assert (type(data[key]) == value)

    assert (re.match(
        r'^[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+$',
        data["IPv6:MeshLocalAddress"]) is not None)
    assert (re.match(r'^[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+$',
                     data["IPv6:MeshLocalPrefix"]) is not None)
    assert (re.match(r'^[a-f0-9]{16}$', data["NCP:HardwareAddress"])
            is not None)
    assert (re.match(r'[a-f0-9]{16}', data["Network:XPANID"]) is not None)

    return True


def node_rloc_check(data):
    assert data is not None

    assert (type(data) == str)

    assert (re.match(
        r'^[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+:[a-f0-9]+$',
        data) is not None)

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

    assert (type(data) == int)

    return True


def node_network_name_check(data):
    assert data is not None

    assert (type(data) == str)

    return True


def node_leader_data_check(data):
    assert data is not None

    assert (type(data) == dict)

    leaderdata_expected_keys = [
        "PartitionId", "Weighting", "DataVersion", "StableDataVersion",
        "LeaderRouterId"
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


def node_test(thread_num):
    url = REST_API_ADDR + "/v1/node"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_check(data) for data in response_data].count(True)

    print(" /v1/node : all {}, valid {} ".format(thread_num, valid))


def node_rloc_test(thread_num):
    url = REST_API_ADDR + "/v1/node/rloc"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_rloc_check(data) for data in response_data].count(True)

    print(" /v1/node/rloc : all {}, valid {} ".format(thread_num, valid))


def node_rloc16_test(thread_num):
    url = REST_API_ADDR + "/v1/node/rloc16"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_rloc16_check(data) for data in response_data].count(True)

    print(" /v1/node/rloc16 : all {}, valid {} ".format(thread_num, valid))


def node_ext_address_test(thread_num):
    url = REST_API_ADDR + "/v1/node/ext-address"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_ext_address_check(data) for data in response_data].count(True)

    print(" /v1/node/ext-address : all {}, valid {} ".format(thread_num, valid))


def node_state_test(thread_num):
    url = REST_API_ADDR + "/v1/node/state"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_state_check(data) for data in response_data].count(True)

    print(" /v1/node/state : all {}, valid {} ".format(thread_num, valid))


def node_network_name_test(thread_num):
    url = REST_API_ADDR + "/v1/node/network-name"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_network_name_check(data) for data in response_data
            ].count(True)

    print(" /v1/node/network-name : all {}, valid {} ".format(
        thread_num, valid))


def node_leader_data_test(thread_num):
    url = REST_API_ADDR + "/v1/node/leader-data"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_leader_data_check(data) for data in response_data].count(True)

    print(" /v1/node/leader-data : all {}, valid {} ".format(thread_num, valid))


def node_num_of_router_test(thread_num):
    url = REST_API_ADDR + "/v1/node/num-of-router"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_num_of_router_check(data) for data in response_data
            ].count(True)

    print(" /v1/node/num-of-router : all {}, valid {} ".format(
        thread_num, valid))


def node_ext_panid_test(thread_num):
    url = REST_API_ADDR + "/v1/node/ext-panid"

    response_data = [None] * thread_num

    create_multi_thread(get_data_from_url, url, thread_num, response_data)

    valid = [node_ext_panid_check(data) for data in response_data].count(True)

    print(" /v1/node/ext-panid : all {}, valid {} ".format(thread_num, valid))


def diagnostics_test(thread_num):
    url = REST_API_ADDR + "/v1/diagnostics"

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

    print(" /v1/diagnostics : all {}, has content {}, valid {} ".format(
        thread_num, has_content, valid))


def error_test(thread_num):
    url = REST_API_ADDR + "/v1/hello"

    response_data = [None] * thread_num

    create_multi_thread(get_error_from_url, url, thread_num, response_data)

    valid = [error404_check(data) for data in response_data].count(True)

    print(" /v1/hello : all {}, valid {} ".format(thread_num, valid))


def commissioner():
    url = REST_API_ADDR + "/v1/networks/current/commission"
    data = {
        'passphase': '123445',
        'pskd': 'ABCDEF',
    }
    data = json.dumps(data).encode()
    response = urllib.request.urlopen(
        urllib.request.Request(url=url, data=data, method='POST'))
    body = response.read()
    data = json.loads(body)
    assert data['pskd'] == 'ABCDEF'

    data = {
        'passphase': '123445',
        'pskd': 'ABCDEF',
    }
    data = json.dumps(data).encode()
    try:
        urllib.request.urlopen(
            urllib.request.Request(url=url, data=data, method='POST'))

    except urllib.error.HTTPError as e:
        body = e.read().decode()
        data = json.loads(body)
        assert data[
            'ErrorDescription'] == 'Failed at commissioner start: Already'
    print("commissioner  pass")


def add_remove_prefix():
    url = REST_API_ADDR + "/v1/networks/current/prefix"
    data = {
        'prefix': 'fd13:22::',
        'defaultRoute': True,
    }
    data = json.dumps(data).encode()
    response = urllib.request.urlopen(
        urllib.request.Request(url=url, data=data, method='POST'))
    body = response.read()
    data = json.loads(body)
    assert data['prefix'] == 'fd13:22::'
    time.sleep(5)

    response = urllib.request.urlopen(urllib.request.Request(url))
    body = response.read()
    data = json.loads(body)
    assert 'fd13:22:0:0' in data

    time.sleep(5)

    data = {'prefix': 'fd13:22::'}
    data = json.dumps(data).encode()
    response = urllib.request.urlopen(
        urllib.request.Request(url=url, data=data, method='DELETE'))
    body = response.read()
    data = json.loads(body)
    assert 'fd13:22:0:0' not in data
    print("add/remove prefix  pass")


def form_network_test():
    url = REST_API_ADDR + "/v1/node/network-name"

    response = urllib.request.urlopen(urllib.request.Request(url))
    body = response.read()
    data = json.loads(body)
    assert data != 'OpenThreadTest1'

    url = REST_API_ADDR + "/v1/networks"
    data = {
        'networkName': 'OpenThreadTest1',
        'extPanId': '1111111122222222',
        'panId': '0x1234',
        'passphrase': '123456',
        'masterKey': '00112233445566778899aabbccddeeff',
        'channel': 15,
        'prefix': 'fd11:22::',
        'defaultRoute': True,
    }
    data = json.dumps(data).encode()

    response = urllib.request.urlopen(
        urllib.request.Request(url=url, data=data, method='POST'))
    body = response.read()
    data = json.loads(body)
    assert data['networkName'] == 'OpenThreadTest1'

    time.sleep(10)

    url = REST_API_ADDR + "/v1/node/network-name"
    response = urllib.request.urlopen(urllib.request.Request(url))
    body = response.read()
    data = json.loads(body)
    assert data == 'OpenThreadTest1'
    print("form network pass")


def scan():
    url = REST_API_ADDR + "/v1/networks"
    response = urllib.request.urlopen(urllib.request.Request(url))
    body = response.read()
    data = json.loads(body)

    flag = False
    for network in data:
        if network['NetworkName'] == 'hello' and network['Channel'] == 20:
            flag = True
            break
    assert flag
    print("scan network pass")


def join():
    url = REST_API_ADDR + "/v1/networks/current"

    data = {
        'extPanId': 'dead00beef00cafe',
        'panId': '0xface',
        'channel': 20,
        'networkName': 'hello',
        'masterKey': '10112233445566778899aabbccddeefe',
        'defaultRoute': False,
        "prefix": 'fd17:22::'
    }
    data = json.dumps(data).encode()

    response = urllib.request.urlopen(
        urllib.request.Request(url=url, data=data, method='PUT'))

    body = response.read()
    data = json.loads(body)
    assert data['masterKey'] == '10112233445566778899aabbccddeefe'

    time.sleep(10)

    url = REST_API_ADDR + "/v1/diagnostics"
    response = urllib.request.urlopen(urllib.request.Request(url))
    body = response.read()
    data = json.loads(body)
    assert len(data) > 1

    print("join network pass")


def main():
    join()
    form_network_test()
    scan()
    add_remove_prefix()
    commissioner()
    node_test(100)
    node_rloc_test(100)
    node_rloc16_test(100)
    node_ext_address_test(100)
    node_state_test(100)
    node_network_name_test(100)
    node_leader_data_test(100)
    node_num_of_router_test(100)
    node_ext_panid_test(100)
    diagnostics_test(10)
    error_test(10)

    return 0


if __name__ == '__main__':
    exit(main())
