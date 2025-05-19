#
#  Copyright (c) 2025, The OpenThread Authors.
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

FROM ubuntu:24.04

ARG GITHUB_REPO="openthread/ot-br-posix"
ARG GIT_COMMIT="HEAD"
ARG TARGETARCH

ENV MDNS_RESPONDER_SOURCE_NAME=mDNSResponder-2600.100.147
ENV S6_OVERLAY_VERSION=3.2.0.2

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

WORKDIR /usr/src

RUN set -x \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
           build-essential \
           ca-certificates \
           cmake \
           curl \
           git \
           ipset \
           iptables \
           ninja-build \
           wget \
    \
    && case "${TARGETARCH}" in \
         amd64) S6_ARCH="x86_64" ;; \
         arm64) S6_ARCH="aarch64" ;; \
         *) echo "Unsupported architecture: ${TARGETARCH}"; exit 1 ;; \
       esac \
    && curl -L -f -s "https://github.com/just-containers/s6-overlay/releases/download/v${S6_OVERLAY_VERSION}/s6-overlay-noarch.tar.xz" \
        | tar Jxvf - -C / \
    && curl -L -f -s "https://github.com/just-containers/s6-overlay/releases/download/v${S6_OVERLAY_VERSION}/s6-overlay-${S6_ARCH}.tar.xz" \
        | tar Jxvf - -C / \
    \
    && git clone --depth 1 -b main https://github.com/"${GITHUB_REPO}".git \
    \
    && (wget --no-check-certificate https://github.com/apple-oss-distributions/mDNSResponder/archive/refs/tags/$MDNS_RESPONDER_SOURCE_NAME.tar.gz \
        && mkdir -p $MDNS_RESPONDER_SOURCE_NAME \
        && tar xvf $MDNS_RESPONDER_SOURCE_NAME.tar.gz -C $MDNS_RESPONDER_SOURCE_NAME --strip-components=1 \
        && cd $MDNS_RESPONDER_SOURCE_NAME \
        && cd mDNSPosix \
        && make os=linux tls=no \
        && make install os=linux tls=no) \
    \
    && cd ot-br-posix \
    && git fetch origin "${GIT_COMMIT}" \
    && git checkout "${GIT_COMMIT}" \
    && git submodule update --depth 1 --init \
    && cmake -GNinja \
           -DBUILD_TESTING=OFF \
           -DCMAKE_INSTALL_PREFIX=/usr \
           -DOTBR_BORDER_ROUTING=ON \
           -DOTBR_BACKBONE_ROUTER=ON \
           -DOTBR_DBUS=OFF \
           -DOTBR_MDNS=mDNSResponder \
           -DOTBR_DNSSD_DISCOVERY_PROXY=ON \
           -DOTBR_SRP_ADVERTISING_PROXY=ON \
           -DOTBR_TREL=ON \
           -DOTBR_NAT64=ON \
           -DOTBR_DNS_UPSTREAM_QUERY=ON \
           -DOT_POSIX_NAT64_CIDR="192.168.255.0/24" \
           -DOT_FIREWALL=ON \
    && ninja \
    && ninja install \
    && cp -r etc/docker/border-router/rootfs/. / \
    && apt-get purge -y --auto-remove \
           build-essential \
           ca-certificates \
           cmake \
           curl \
           git \
           ninja-build \
           wget \
    && rm -rf /var/lib/apt/lists/* \
    && rm -rf /usr/src/*

ENTRYPOINT ["/init"]
