#
# Copyright (c) 2016 Nest Labs, Inc.
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

FROM ubuntu:trusty

ENV CONNMAN_INCLUDE_ARCHIVE=https://gist.github.com/darconeous/d1d9bc39e0758e45a1d7/raw/ef9b01ac378a9b2e92031c846c4f6b5f94abab53/connman-include.tar.bz2

RUN apt-get -y update \
	&& DEBIAN_FRONTEND=noninteractive \
			apt-get install -y -q --no-install-recommends \
				libglib2.0-dev \
				libdbus-1-dev \
				libdbus-1-dev \
				libreadline-dev \
				libtool \
				autoconf-archive \
				dbus \
				xtables-addons-source \
				net-tools \
				usbutils \
				vim \
				man \
				bsdtar \
				gcc g++ \
				pkg-config \
				autoconf \
				autoconf-archive \
				automake

ADD $CONNMAN_INCLUDE_ARCHIVE /

RUN apt-get -y update \
	&& DEBIAN_FRONTEND=noninteractive \
			apt-get install -y -q --no-install-recommends \
				libboost-dev \
				libboost-signals-dev

RUN tar xvjf /connman-include.tar.bz2 -C / && rm /connman-include.tar.bz2
