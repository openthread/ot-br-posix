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
import os
import re
import time
from threading import Thread

rest_api_addr = "http://127.0.0.1:8081"


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
            "childTable", "channelPages", "maxChildTimeout", "threadVersion",
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
            "rxOnWhenIdle", "fullThreadDevice", "fullNetworkData"
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

        assert (re.match(r'^[a-f0-9]{44}$', diag["networkData"]) is not None)

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


def node_state_put_invalid_body_test():
    """Regression test for https://github.com/openthread/ot-br-posix/issues/3522

    A PUT request that fails to process (e.g. an invalid body) must return
    its real error status (400 Bad Request here) instead of being masked as
    405 Method Not Allowed by RoutingErrorHandler.
    """
    url = rest_api_addr + "/node/state"

    # Not valid JSON (missing quotes around the string value), so the
    # handler rejects it with 400 before touching any device state.
    request = urllib.request.Request(url, data=b'enable', method='PUT')

    try:
        urllib.request.urlopen(request)
        assert False
    except urllib.error.HTTPError as e:
        assert (e.code == 400)

    print(" PUT /node/state (invalid body) : status {}, expected 400".format(400))


def node_state_put_unsupported_method_test():
    """A method that genuinely isn't supported (e.g. PATCH) on a PUT-capable
    endpoint must still return 405.
    """
    url = rest_api_addr + "/node/state"

    request = urllib.request.Request(url, method='PATCH')

    try:
        urllib.request.urlopen(request)
        assert False
    except urllib.error.HTTPError as e:
        assert (e.code == 405)

    print(" PATCH /node/state : status {}, expected 405".format(405))


def node_ext_address_put_method_not_allowed_test():
    """A PUT to an endpoint that never registers a PUT handler (e.g.
    /node/ext-address, GET-only) is a genuinely unsupported method on a
    route that does exist, so RouteRegistry must report it as 405 with an
    Allow header listing the methods that are actually registered - not a
    plain 404. Regression test for
    https://github.com/openthread/ot-br-posix/issues/3529.
    """
    url = rest_api_addr + "/node/ext-address"

    request = urllib.request.Request(url, data=b'"0000000000000000"', method='PUT')

    try:
        urllib.request.urlopen(request)
        assert False
    except urllib.error.HTTPError as e:
        assert (e.code == 405)
        assert (e.headers.get("Allow") == "GET, OPTIONS")
        assert e.headers.get("Content-Type") == "application/json"
        body = json.loads(e.read().decode('utf-8'))
        assert body["status"] == 405
        assert body["title"] == "Method Not Allowed"
        assert body.get("detail") == "method not supported"

    print(" PUT /node/ext-address (registered route, wrong method) : status {}, expected 405".format(405))


def node_state_delete_method_not_allowed_test():
    """DELETE on /node/state (which only registers GET and PUT) must be
    rejected with 405, and the Allow header must list every method actually
    registered on that route, not just the one that was tried.
    """
    request = urllib.request.Request(rest_api_addr + "/node/state", method='DELETE')

    try:
        urllib.request.urlopen(request)
        assert False
    except urllib.error.HTTPError as e:
        assert (e.code == 405)
        assert (e.headers.get("Allow") == "GET, PUT, OPTIONS")
        assert e.headers.get("Content-Type") == "application/json"
        body = json.loads(e.read().decode('utf-8'))
        assert body["status"] == 405
        assert body["title"] == "Method Not Allowed"
        assert body.get("detail") == "method not supported"

    print(" DELETE /node/state : status {}, expected 405, Allow 'GET, PUT, OPTIONS'".format(405))


def node_state_options_test():
    """OPTIONS on a route with more than one registered method must report
    all of them via the Allow header (in GET/POST/PUT/DELETE order), plus
    OPTIONS itself.
    """
    req = urllib.request.Request(rest_api_addr + "/node/state", method='OPTIONS')
    with urllib.request.urlopen(req) as response:
        assert response.status == 204
        assert response.headers.get("Allow") == "GET, PUT, OPTIONS"

    print(" OPTIONS /node/state : status 204, Allow 'GET, PUT, OPTIONS'")


def node_ext_address_options_test():
    """OPTIONS on a GET-only route must report only GET plus OPTIONS."""
    req = urllib.request.Request(rest_api_addr + "/node/ext-address", method='OPTIONS')
    with urllib.request.urlopen(req) as response:
        assert response.status == 204
        assert response.headers.get("Allow") == "GET, OPTIONS"

    print(" OPTIONS /node/ext-address : status 204, Allow 'GET, OPTIONS'")


def node_options_test():
    """OPTIONS on /node (GET+DELETE) exercises the DELETE branch of
    RouteRegistry::BuildMethodsString directly, so coverage of that branch
    doesn't depend on some other, unrelated test suite (e.g. schemathesis
    against /api/*) happening to touch a DELETE-bearing route.
    """
    req = urllib.request.Request(rest_api_addr + "/node", method='OPTIONS')
    with urllib.request.urlopen(req) as response:
        assert response.status == 204
        assert response.headers.get("Allow") == "GET, DELETE, OPTIONS"

    print(" OPTIONS /node : status 204, Allow 'GET, DELETE, OPTIONS'")


def api_actions_item_not_found_test():
    """GET /api/actions/unknown UUID: the route AND the method
    match a registered handler (ApiActionsItemGetHandler), but the item itself
    doesn't exist, so the handler legitimately returns 404 on its own - this must
    NOT be rewritten or otherwise touched by RoutingErrorHandler's routing-miss
    logic, which only applies to routes/methods that were never registered.
    """
    url = rest_api_addr + "/api/actions/unknown-uuid"
    request = urllib.request.Request(url, headers={'Accept': 'application/vnd.api+json'})

    try:
        urllib.request.urlopen(request)
        assert False, "expected HTTP 404, but request succeeded"
    except urllib.error.HTTPError as e:
        assert e.code == 404, "expected HTTP 404, got {}".format(e.code)
        assert e.headers.get("Content-Type") == "application/json"
        body = json.loads(e.read().decode('utf-8'))
        assert body["status"] == 404
        assert body["title"] == "Not Found"

    print(" GET /api/actions/unknown-uuid : status 404, expected 404 (matched route/method, app-level miss)")


def unknown_route_test():
    """A path with no registered route at all is a real 404, regardless of
    method (including OPTIONS, which must not be handled generically since
    RouteRegistry has no route to report methods for), and must not carry
    an Allow header.
    """
    url = rest_api_addr + "/node/this-path-does-not-exist"

    for method in ("GET", "OPTIONS", "POST", "PUT", "DELETE"):
        request = urllib.request.Request(url, method=method)
        try:
            urllib.request.urlopen(request)
            assert False, "expected HTTP 404 for {}, but request succeeded".format(method)
        except urllib.error.HTTPError as e:
            assert e.code == 404, "expected HTTP 404 for {}, got {}".format(method, e.code)
            assert e.headers.get("Content-Type") == "application/json"
            body = json.loads(e.read().decode('utf-8'))
            assert body["status"] == 404
            assert body["title"] == "Not Found"
            assert e.headers.get("Allow") is None, "unexpected Allow header on a real 404 for {}".format(method)

        print(" {} /node/this-path-does-not-exist : status 404, no Allow header".format(method))


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


def get_expected_rest_api_version():
    # Regression test for https://github.com/openthread/ot-br-posix/issues/3535:
    # verify that the /.well-known/thread/br-rest discovery endpoint correctly
    # advertises the compile-time OTBR_REST_API_VERSION constant from version.hpp.
    # Parsing the constant directly from the header avoids hardcoding a version string
    # in this test that would require maintenance on future version bumps.
    cur_dir = os.path.dirname(os.path.abspath(__file__))
    version_header = os.path.normpath(os.path.join(cur_dir, "..", "..", "src", "rest", "version.hpp"))

    with open(version_header, encoding="utf-8") as f:
        contents = f.read()

    match = re.search(r'#define\s+OTBR_REST_API_VERSION\s+"([^"]+)"', contents)
    assert match is not None, "OTBR_REST_API_VERSION not found in {}".format(version_header)

    return match.group(1)


def well_known_thread_check(data, expected_version):
    assert data is not None

    assert "api" in data and isinstance(data["api"], dict)
    assert data["api"].get("version") == expected_version
    assert data["api"].get("base") == "/api/"

    assert "links" in data and isinstance(data["links"], list)
    links = {link.get("rel"): link for link in data["links"] if isinstance(link, dict)}

    # Per src/rest/openapi.yaml, the endpoint must advertise itself plus every
    # JSON:API entry point, each typed with the content type it actually serves.
    expected_links = {
        "self": ("/.well-known/thread/br-rest", "application/json"),
        "node": ("/api/node", "application/vnd.api+json"),
        "task": ("/api/actions", "application/vnd.api+json"),
        "device": ("/api/devices", "application/vnd.api+json"),
        "diagnostic": ("/api/diagnostics", "application/vnd.api+json"),
    }
    # Compare against the raw list length (not len(links), which the dict comprehension
    # above would have already deduplicated) to catch duplicate rel entries as well as
    # unexpected extra links.
    assert len(data["links"]) == len(expected_links), "unexpected or duplicate links: {}".format(
        [link.get("rel") for link in data["links"]])

    for rel, (href, content_type) in expected_links.items():
        link = links.get(rel)
        assert link is not None, "missing link with rel={}".format(rel)
        assert link.get("href") == href
        assert link.get("type") == [content_type]

    return True


def well_known_thread_get(url, result, index):
    req = urllib.request.Request(url, headers={'Accept': 'application/json'})
    with urllib.request.urlopen(req) as response:
        assert response.status == 200
        assert response.headers.get_content_type() == "application/json"
        result[index] = json.loads(response.read())


def well_known_thread_options_test():
    req = urllib.request.Request(rest_api_addr + "/.well-known/thread/br-rest", method='OPTIONS')
    with urllib.request.urlopen(req) as response:
        assert response.status == 204
        assert response.headers.get("Allow") == "GET, OPTIONS"


def well_known_thread_method_not_allowed_test():
    """The discovery endpoint only supports GET and OPTIONS; any other method
    (e.g. DELETE, POST, PUT) must be rejected with 405.
    """
    url = rest_api_addr + "/.well-known/thread/br-rest"

    for method in ("DELETE", "POST", "PUT"):
        request = urllib.request.Request(url, method=method)
        try:
            urllib.request.urlopen(request)
            assert False, "expected HTTP 405 for {}, but request succeeded".format(method)
        except urllib.error.HTTPError as e:
            assert e.code == 405, "expected HTTP 405 for {}, got {}".format(method, e.code)

        print(" {} /.well-known/thread/br-rest : status {}, expected 405".format(method, 405))


def well_known_thread_test(thread_num=1):
    url = rest_api_addr + "/.well-known/thread/br-rest"
    expected_version = get_expected_rest_api_version()

    response_data = [None] * thread_num
    create_multi_thread(well_known_thread_get, url, thread_num, response_data)

    valid = [well_known_thread_check(data, expected_version) for data in response_data].count(True)

    well_known_thread_options_test()
    well_known_thread_method_not_allowed_test()

    print(" /.well-known/thread/br-rest : all {}, valid {} (version {})".format(thread_num, valid, expected_version))


def epskc_test():
    state_url = rest_api_addr + "/node/ba-epskc/state"
    key_url = rest_api_addr + "/node/ba-epskc/key"

    def put_state(value):
        req = urllib.request.Request(state_url, data=json.dumps(value).encode(), method='PUT')
        req.add_header('Content-Type', 'application/json')
        urllib.request.urlopen(req)

    def get_state():
        return json.loads(urllib.request.urlopen(state_url).read())

    def get_key_status():
        return json.loads(urllib.request.urlopen(key_url).read())

    def post_key(payload):
        req = urllib.request.Request(key_url, data=json.dumps(payload).encode(), method='POST')
        req.add_header('Content-Type', 'application/json')
        return urllib.request.urlopen(req)

    def expect_http_error(code, action):
        try:
            action()
            raise AssertionError("expected HTTP {}".format(code))
        except urllib.error.HTTPError as err:
            assert err.code == code

    # Regression test for https://github.com/openthread/ot-br-posix/issues/3522: an invalid
    # PUT body must return its real error status (400) rather than being masked as 405.
    expect_http_error(400, lambda: put_state("toggle"))

    # Feature disabled: activation must be refused.
    put_state("disable")
    assert get_state() == "disabled"

    expect_http_error(409, lambda: urllib.request.urlopen(urllib.request.Request(key_url, data=b'{}', method='POST')))

    # Enable, activate, check, deactivate, disable.
    put_state("enable")
    assert get_state() == "enabled"

    status = get_key_status()
    assert status["state"] == "stopped"

    # Invalid payloads must be rejected.
    expect_http_error(400, lambda: post_key({"lifetime": "3600"}))
    expect_http_error(400, lambda: post_key({"port": "49191"}))
    expect_http_error(400, lambda: post_key({"lifetime": 4294967295}))

    # Custom valid parameters (custom lifetime with auto-assigned port).
    data = json.loads(post_key({"lifetime": 30000, "port": 0}).read())
    assert len(data["tap"]) == 9 and data["tap"].isdigit()
    assert 0 < data["port"] <= 65535

    status = get_key_status()
    assert status["state"] == "started" and status["port"] == data["port"]

    # A second activation attempt while one is already active must be refused.
    expect_http_error(409, lambda: urllib.request.urlopen(urllib.request.Request(key_url, data=b'{}', method='POST')))

    urllib.request.urlopen(urllib.request.Request(key_url, method='DELETE'))
    assert get_key_status()["state"] == "stopped"

    put_state("disable")
    assert get_state() == "disabled"

    print(" /node/ba-epskc : OK")


def main():
    node_test(200)
    node_rloc_test(200)
    node_rloc16_test(200)
    node_ext_address_test(200)
    node_state_test(200)
    node_state_put_invalid_body_test()
    node_state_put_unsupported_method_test()
    node_ext_address_put_method_not_allowed_test()
    node_state_delete_method_not_allowed_test()
    node_state_options_test()
    node_ext_address_options_test()
    node_options_test()
    api_actions_item_not_found_test()
    unknown_route_test()
    node_network_name_test(200)
    node_state_test_attached()  # wait for attached state
    node_leader_data_test(200)
    node_num_of_router_test(200)
    node_ext_panid_test(200)
    node_coprocessor_version_test(200)
    # diagnostics_test(20)  # partly replaced with restjsonapi tests
    error_test(10)
    well_known_thread_test(20)
    epskc_test()

    return 0


if __name__ == '__main__':
    exit(main())
