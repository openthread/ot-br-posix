#  Copyright (c) 2018, The OpenThread Authors.
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

FROM ubuntu

ENV DEBIAN_FRONTEND noninteractive
ENV PLATFORM ubuntu
ENV RELEASE 1
ENV NAT64 1
ENV DNS64 1

COPY . /app
WORKDIR /app

# Required during build or run
ENV OTBR_DOCKER_REQS sudo

# Required during build, could be removed
ENV OTBR_DOCKER_DEPS git

# Required and installed during build (script/bootstrap), could be removed
ENV OTBR_BUILD_DEPS \
  apt-utils \
  autoconf \
  autoconf-archive \
  automake \
  build-essential \
  ctags \
  libtool \
  wget

# Required and installed during build (script/bootstrap) when RELEASE=1, could be removed
ENV OTBR_NORELEASE_DEPS \
  cmake \
  cpputest \
  doxygen

RUN apt-get update \
  && apt-get install -y $OTBR_DOCKER_REQS $OTBR_DOCKER_DEPS \
  && git reset --hard && git clean -xffd \
  && ./script/bootstrap \
  && ./script/setup \
  && chmod 644 /etc/bind/named.conf.options \
  && mv ./script /tmp \
  && mv ./etc /tmp \
  && find . -delete \
  && rm -rf /usr/include \
  && mv /tmp/script . \
  && mv /tmp/etc . \
  && apt-get purge -y --auto-remove -o APT::AutoRemove::RecommendsImportant=false $OTBR_DOCKER_DEPS \
  && apt-get purge -y --auto-remove -o APT::AutoRemove::RecommendsImportant=false $OTBR_BUILD_DEPS  \
  && ([ "${RELEASE}" = 1 ] ||  apt-get purge -y --auto-remove -o APT::AutoRemove::RecommendsImportant=false "$OTBR_NORELEASE_DEPS";) \
  && rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["/app/etc/docker/docker_entrypoint.sh"]

EXPOSE 80
