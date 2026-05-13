#!/bin/bash
#
#  Copyright (c) 2026, The OpenThread Authors.
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

set -euxo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly SCRIPT_DIR
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
readonly REPO_ROOT

#---------------------------------------
# Configurations
#---------------------------------------
OTBR_DOCKER_IMAGE="${OTBR_DOCKER_IMAGE:-openthread/border-router}"
readonly OTBR_DOCKER_IMAGE

OTBR_DOCKER_FILE="${OTBR_DOCKER_FILE:-${REPO_ROOT}/etc/docker/border-router/Dockerfile}"
readonly OTBR_DOCKER_FILE

INFRA_NET_NAME="infrastructure-link"
readonly INFRA_NET_NAME

TEST_CLIENT_IMAGE="test-client-mdnsresponder"
readonly TEST_CLIENT_IMAGE

CONTAINER_NAME="otbr-test-container"
readonly CONTAINER_NAME

ABS_TOP_OT_SRCDIR="${REPO_ROOT}/third_party/openthread/repo"
readonly ABS_TOP_OT_SRCDIR

ABS_TOP_OT_BUILDDIR="${REPO_ROOT}/build/simulation"
readonly ABS_TOP_OT_BUILDDIR

#----------------------------------------
# Helper functions
#----------------------------------------
die()
{
    echo " *** ERROR: $*"
    exit 1
}

test_teardown()
{
    echo "--- Tearing down test environment ---"
    echo "Active and inactive containers:"
    docker ps -a || true
    echo "Container logs:"
    for c in "${CONTAINER_NAME}" "otbr-test-container-1" "otbr-test-container-2" "test-client"; do
        docker logs "$c" || true
    done

    echo "Agent logs:"
    for c in "${CONTAINER_NAME}" "otbr-test-container-1" "otbr-test-container-2"; do
        docker exec -i "$c" cat /var/log/otbr-agent.log || true
    done

    for c in "${CONTAINER_NAME}" "otbr-test-container-1" "otbr-test-container-2" "test-client"; do
        docker stop "$c" || true
        docker rm "$c" || true
    done

    for c in $(docker network inspect "${INFRA_NET_NAME}" -f '{{range $id, $el := .Containers}}{{$id}} {{end}}' 2>/dev/null || true); do
        docker stop "$c" || true
        docker rm "$c" || true
    done
    sleep 5
    docker network rm "${INFRA_NET_NAME}" || true
    pkill -f ot-rcp || true
    pkill -f ot-cli-ftd || true
    pkill -f socat || true
    rm -vf /tmp/otbr-pty1 /tmp/otbr-pty2 || true
    rm -vf "${REPO_ROOT}"/*.flash* || true
}

test_setup()
{
    echo "--- Setting up test environment ---"
    rm -vf "${REPO_ROOT}"/*.flash* || true

    for c in $(docker network inspect "${INFRA_NET_NAME}" -f '{{range $id, $el := .Containers}}{{$id}} {{end}}' 2>/dev/null || true); do
        docker stop "$c" || true
        docker rm "$c" || true
    done
    sleep 5

    echo "Waiting for local Docker daemon to be ready..."
    until docker info >/dev/null 2>&1; do
        sleep 1
    done

    echo "Adding iptables rule to allow port 9000 traffic..."
    iptables -I INPUT 1 -p tcp --dport 9000 -j ACCEPT || true

    # 1. Build the production Docker image
    echo "Building production border-router image..."
    docker build --build-arg OTBR_OPTIONS="-DOTBR_BACKBONE_ROUTER=OFF" -t "${OTBR_DOCKER_IMAGE}" -f "${OTBR_DOCKER_FILE}" "${REPO_ROOT}"

    # 2. Create the IPv6-enabled Docker bridge network for infrastructure-link
    if ! docker network inspect "${INFRA_NET_NAME}" >/dev/null 2>&1; then
        echo "Creating ${INFRA_NET_NAME} Docker bridge network..."
        docker network create --driver bridge --ipv6 --subnet fd00:db8:1::/64 "${INFRA_NET_NAME}"
    else
        echo "Network ${INFRA_NET_NAME} already exists."
    fi

    # 3. Build the test client image with mdns-scan and ping/ndisc6 tools
    echo "Building test client image with mdns-scan and ping/ndisc6 tools..."
    docker build -t "${TEST_CLIENT_IMAGE}" - <<EOF
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y mdns-scan iputils-ping ndisc6 python3-zeroconf iproute2 --no-install-recommends && rm -rf /var/lib/apt/lists/*
ENTRYPOINT ["mdns-scan"]
EOF

    # 4. Build simulated OpenThread CLI and RCP binaries
    echo "Building simulated binaries..."
    rm -rf "${ABS_TOP_OT_BUILDDIR}"
    OT_CMAKE_BUILD_DIR=${ABS_TOP_OT_BUILDDIR}/rcp "${ABS_TOP_OT_SRCDIR}"/script/cmake-build simulation \
        -DOT_MTD=OFF -DOT_FTD=OFF -DOT_APP_CLI=OFF -DOT_APP_NCP=OFF -DOT_APP_RCP=ON \
        -DBUILD_TESTING=OFF

    OT_CMAKE_BUILD_DIR=${ABS_TOP_OT_BUILDDIR}/cli "${ABS_TOP_OT_SRCDIR}"/script/cmake-build simulation \
        -DOT_MTD=OFF -DOT_RCP=OFF -DOT_APP_NCP=OFF -DOT_APP_RCP=OFF \
        -DOT_BORDER_ROUTING=OFF -DOT_MLR=ON \
        -DBUILD_TESTING=OFF
}

test_run()
{
    trap test_teardown EXIT

    echo "--- Running integration expect scripts ---"
    export EXP_OTBR_DOCKER_IMAGE="${OTBR_DOCKER_IMAGE}"
    export EXP_OTBR_AGENT_PATH="${REPO_ROOT}/build/src/agent/otbr-agent"
    export EXP_TUN_NAME="wpan0"
    export EXP_LEADER_NODE_ID=1
    export EXP_REPO_ROOT="${REPO_ROOT}"
    export EXP_TEST_CLIENT_IMAGE="${TEST_CLIENT_IMAGE}"

    local ot_cli
    local ot_rcp
    ot_cli=$(find "${ABS_TOP_OT_BUILDDIR}/cli" -name "ot-cli-ftd")
    ot_rcp=$(find "${ABS_TOP_OT_BUILDDIR}/rcp" -name "ot-rcp")
    [ -x "$ot_cli" ] || die "cli binary not found"
    [ -x "$ot_rcp" ] || die "rcp binary not found"

    export EXP_OT_CLI_PATH="$ot_cli"
    export EXP_OT_RCP_PATH="$ot_rcp"

    echo "--- Running DNS-SD integration test ---"
    expect -df "${SCRIPT_DIR}/expect/dind_dns_sd.exp"

    echo "--- Running TREL integration test ---"
    expect -df "${SCRIPT_DIR}/expect/dind_trel.exp"
}

main()
{
    test_setup
    test_run
}

main "$@"
