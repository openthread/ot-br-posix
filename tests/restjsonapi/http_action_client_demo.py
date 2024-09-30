#!/usr/bin/python3

# Standard imports
import json
import logging
import logging.handlers
import os, sys
from time import sleep, time

# Third party imports
import requests

# Logging setup
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)
# Create logger to stdout
std_handler = logging.StreamHandler(sys.stdout)
std_handler.setLevel(logging.DEBUG)

# Add handler to logger
logger.addHandler(std_handler)

# set environment variables
OTBR_SERVER_URL = "127.0.0.1"
OTBR_REST_API_PORT = "8081"

logger.info(f"Found OTBR server configuration for {OTBR_SERVER_URL}")

headers = {"Content-type": "application/vnd.api+json"}
accept_headers = {"Accept": "application/vnd.api+json"}

base_url = "http://" + OTBR_SERVER_URL + ":" + OTBR_REST_API_PORT + "/api/"
url_actions = base_url + "actions"

MAX_RETRIES = 3
SKIP_RXOFFWHENIDLE = True  # requests to rx-off-when-idle devices may be very slow

class Action:

    def __init__(self, destination, eui64="fedcba9876543210", timeout=30):

        self.action_type = "notDefined"
        self.result_type = "notDefined"
        self.action_id = None
        self.result_id = None
        self.url = None
        self.retries = 0
        self.status = None

        self.destination = destination
        self.eui64 = eui64
        self.timeout = timeout

    def __repr__(self):
        return (
            f"type={self.action_type}, id={self.action_id}, dest={self.destination}, "
            + f"result_id={self.result_id}, retries={self.retries}, status={self.status}"
        )

    def post_action(self, url=url_actions):

        body = self._default_action()
        self.url = url
        response = requests.post(url, headers=headers, data=json.dumps(body), timeout=10)
        if response.ok:
            self.action_id = response.json()["data"][0]["id"]
        else:
            logger.warning(f"{response.status_code} - {response.reason} - {self._default_action()}")

    def get_action(self):

        if self.action_id:
            response = requests.get(
                self.url + "/" + self.action_id, headers=accept_headers, timeout=10
            )
        else:
            return

        action = response.json()["data"]
        self.status = action["attributes"]["status"]
        match self.status:
            case "completed":
                try:
                    self.result_id = action["relationships"]["result"]["data"]["id"]
                except KeyError:
                    logger.exception("Error")
            case "stopped" | "failed":
                # retry
                if self.retries <= MAX_RETRIES:
                    self.retries += 1
                    self.post_action()
                    self.status = "pending"
                else:
                    self.status = "stopped"
                # logger.info(f"stopped {self.destination}")
            case "stored":
                # nothing to do
                pass
            case _:
                # wait longer
                # logger.info(f"wait longer for {self.destination}")
                pass

    def get_results(self) -> bool:

        if self.result_id and self.status and self.status not in "stored":
            response = None
            url_diagnostic_id = base_url + self.result_type + "/" + self.result_id
            response = requests.get(url_diagnostic_id, headers=accept_headers, timeout=2)
            if not response.ok:
                logger.warning(response.reason)
                return False

            try:
                result = response.json()["data"]["attributes"]
                logger.info(f"{self.result_id}: {json.dumps(result)}")
                self.store_result(result)
                self.status = "stored"
                return True
            except Exception:
                logger.exception("Failed getting results")
        return False

    def store_result(self, result):
        # do something
        raise NotImplementedError()

    def _default_action(self):
        raise NotImplementedError()


class EnergyScanAction(Action):
    def __init__(
        self,
        destination,
        eui64="fedcba9876543210",
        channel_mask=[
            11,
            12,
            13,
            14,
            15,
            16,
            17,
            18,
            19,
            20,
            21,
            22,
            23,
            24,
            25,
            26,
        ],
        count=2,
        period=32,
        duration=16,
        timeout=30,
    ):
        super().__init__(destination, eui64, timeout)

        self.action_type = "getEnergyScanTask"
        self.result_type = "diagnostics"

        self.count = count
        self.period = period
        self.scan_duration = duration
        self.channel_mask = channel_mask

    def _default_action(self):
        return {
            "data": [
                {
                    "type": self.action_type,
                    "attributes": {
                        "destination": str(self.destination),
                        "channelMask": self.channel_mask,
                        "count": self.count,
                        "period": self.period,
                        "scanDuration": self.scan_duration,
                        "timeout": self.timeout,
                    },
                },
            ]
        }

    def store_result(self, result):
        logger.info(result)


class NetworkDiagnosticsAction(Action):
    def __init__(
        self, destination, eui64="fedcba9876543210", types=["extAddress", "eui64"], timeout=30
    ):

        super().__init__(destination, eui64, timeout)

        self.action_type = "getNetworkDiagnosticTask"
        self.result_type = "diagnostics"

        self.types = types

    def _default_action(self):
        return {
            "data": [
                {
                    "type": self.action_type,
                    "attributes": {
                        "destination": str(self.destination),
                        "types": self.types,
                        "timeout": self.timeout,
                    },
                },
            ]
        }

    def store_result(self, result):
        logger.info(result)


def update_device_collection():
    """If successfull, returns json:api doc."""

    url_devices = "http://" + OTBR_SERVER_URL + ":" + OTBR_REST_API_PORT + "/api/devices"

    retry = 0
    while retry <= 5:
        try:
            response = requests.post(url_devices, headers=accept_headers, timeout=10)
            break
        except (
            requests.RequestException,
            requests.ReadTimeout,
            requests.Timeout,
            TimeoutError,
        ) as error:
            logger.error(
                f"{repr(error)} bootstraping device collection. Going for retry {retry+1}."
            )
            sleep(5)
            retry += 1

    if not response.status_code == 200:
        logger.info(response.json())
    else:
        logger.info(f"Got Device Collection {response.json()['meta']}")

    return response.json()


def get_device_collection():
    """If successfull, returns json:api doc."""

    url_devices = "http://" + OTBR_SERVER_URL + ":" + OTBR_REST_API_PORT + "/api/devices"
    response = requests.get(url_devices, headers=accept_headers, timeout=10)
    return response.json()


def get_actions(action_id):
    """Get a action item by its action_id."""
    url_action_id = url_actions + action_id

    response = requests.get(url_action_id, headers=accept_headers, timeout=10)
    return response.json()


def delete_actions():
    logger.warning("Deleting actions on server.")
    requests.delete(url_actions, headers=accept_headers, timeout=10)


def check_completeness(devices):
    """Check all devices have a valid mlEidIid attribute value."""

    for device in devices:
        if "0000000000000000" in device["attributes"]["mlEidIid"]:
            logger.info(f"Missing ML-EID IID of device {device['id']}")
            return False
    return True


def prepare_device_coll(max_device_count):
    """Check collection of devices contains expected minimal number of items."""
    
    logger.info(f"Get/update network inventory from api/devices ...")
    retry = 0

    device_coll = get_device_collection()
    device_count = device_coll["meta"]["collection"]["total"]

    haveall_mleidiid = False
    while (device_count < max_device_count) or not haveall_mleidiid:
        device_coll = update_device_collection()
        try:
            device_count = device_coll["meta"]["collection"]["total"]
            haveall_mleidiid = check_completeness(device_coll["data"])
            if not haveall_mleidiid:
                retry = min(retry + 1, 60)
                retry_delay = 2 * retry
                logger.info(f" - delay retry {retry} by {retry_delay}s ...")
                sleep(retry_delay)
        except KeyError:
            retry = min(retry + 1, 60)
            retry_delay = 2 * retry
            logger.info(f"Devices {device_count}/{max_device_count} - delay retry by {retry_delay}s ...")
            sleep(retry_delay)
        if retry > 6:
            break


    logger.info(f"Devices {device_count}/{max_device_count}. {len(device_coll['data'])} in device collection.")

    return device_coll["data"]


def _process_pending_actions(active, completed):
    # check previous actions are completed and get results
    active_action_dest = ""
    for action in active:
        action.get_action()
        if action.get_results() or action.status not in ["pending", "active"]:
            completed.append(action)
        if action.status == "active":
            active_action_dest = action.destination

    for action in completed:
        try:
            active.remove(action)
        except ValueError:
            # already removed
            pass
        except Exception:
            logger.exception("Error")

    if len(active) > 0:
        logger.info(
            f"Results: {len(active)} pending, {len(completed)} complete; currently active action to destination {active_action_dest}."
        )
    else:
        logger.info("Completed.")

    return (active, completed)


def run_energy_scan(count_of_devices, channel_mask, count, period, duration):

    logger.info("Start energy scan.")

    device_set = prepare_device_coll(count_of_devices)

    active_actions = []
    completed_actions = []

    timeout = time() + (count_of_devices * 15)

    # to keep the test simple, we start with an empty collection
    delete_actions()
    actions = get_actions("")
    if actions["meta"]["collection"]["total"] != 0:
        logger.warning("Actions not empty.")

    for device in device_set:
        if not device["attributes"]["mode"]["rxOnWhenIdle"] and SKIP_RXOFFWHENIDLE:
            logger.info("Skip rx-off-when-idle child ...")
            continue
        try:
            eui = device["attributes"]["eui64"]
        except KeyError:
            eui = "0000000000000000"
        act = EnergyScanAction(device["id"], eui, channel_mask, count, period, duration)
        act.post_action()
        active_actions.append(act)

        # the best case duration of the energy scans at one device
        wait = act.count * (act.scan_duration + act.period) * len(act.channel_mask) / 1000

        # Only a single scan runs at a time
        # We may post all the actions,
        # but we want to wait a bit and not create bursts.
        logger.info(f"Wait for completion after {wait} s")
        sleep(wait)

    while len(active_actions) > 0:

        (active_actions, completed_actions) = _process_pending_actions(
            active_actions, completed_actions
        )

        if time() > timeout:
            logger.warning("Timeout for energy scan.")
            for action in active_actions:
                logger.warning(action)
            break
        if len(active_actions) > 0:
            sleep(10)

    return completed_actions


def run_network_diagnostics(count_of_devices, types):

    logger.info("Start network diagnostics.")

    device_set = prepare_device_coll(count_of_devices)

    active_actions = []
    completed_actions = []

    timeout = time() + (count_of_devices * 15)

    # to keep the test simple, we start with an empty collection
    delete_actions()
    actions = get_actions("")
    if actions["meta"]["collection"]["total"] != 0:
        logger.warning("Actions not empty.")

    for device in device_set:
        if not device["attributes"]["mode"]["rxOnWhenIdle"] and SKIP_RXOFFWHENIDLE:
            logger.info("Skip rx-off-when-idle child ...")
            continue
        try:
            eui = device["attributes"]["eui64"]
        except KeyError:
            eui = "0000000000000000"
        act = NetworkDiagnosticsAction(device["id"], eui, types)
        act.post_action()
        active_actions.append(act)

    while len(active_actions) > 0:
        sleep(0.5)
        (active_actions, completed_actions) = _process_pending_actions(
            active_actions, completed_actions
        )

        if time() > timeout:
            logger.warning("Timeout for network diagnostics.")
            for action in active_actions:
                logger.warning(action)
            break

    return completed_actions


if __name__ == "__main__":
    # a short test script
    DEVICE_COUNT = 15  # expected minimal device count
    ACTION_TYPE = 1  # 0: energy scan, 1: networkDiagnostic
    
    #logger = logging.getLogger(__name__)

    logger.info(f"Found server configuration for {OTBR_SERVER_URL}")

    for i in range(1):
        logger.info(f"Iteration: {i}")
        start_time = time()
        match ACTION_TYPE:
            case 0:
                compl_actions = run_energy_scan(DEVICE_COUNT, [24, 25, 26], 2, 32, 16)
            case 1:
                compl_actions = run_network_diagnostics(
                    DEVICE_COUNT,
                    [
                        "extAddress",
                        "rloc16",
                        "mode",
                        "timeout",
                        "leaderData",
                        "ipv6Addresses",
                        "maxChildTimeout",
                        "version",
                        "vendorName",
                        "vendorModel",
                        "vendorSwVersion",
                        "threadStackVersion",
                        "children",
                        "childIpv6Addresses",
                        "routerNeighbor",
                    ],
                )
            case _:
                logger.warning("Set a valid value for ACTION_TYPE.")
        # log final status
        for action in compl_actions:
            logger.info(action)

        actions = get_actions("")
        logger.info(f'Action items on server: {actions["meta"]["collection"]["total"]}')

        logger.info(f"Iteration took {time()-start_time} seconds.")
