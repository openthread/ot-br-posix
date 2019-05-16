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
ENV RELEASE 1
ENV NAT64 1
ENV DNS64 1

COPY . /app

WORKDIR /app

RUN apt-get update && apt-get install -y git lsb-core sudo

RUN git reset --hard && git clean -xffd
RUN ./script/bootstrap && ./script/setup
RUN chmod 644 /etc/bind/named.conf.options
RUN mv ./script /tmp && mv ./etc /tmp
RUN find . -delete && rm -rf /usr/include
RUN mv /tmp/script . && mv /tmp/etc .
RUN apt-get autoremove -y -o APT::Autoremove::RecommendsImportant=0 -o APT::Autoremove::SuggestsImportant=0 && rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["/app/etc/docker/docker_entrypoint.sh"]

EXPOSE 80
