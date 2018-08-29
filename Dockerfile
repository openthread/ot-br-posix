FROM ubuntu

WORKDIR /app/borderrouter

ADD . /app/borderrouter

RUN apt-get -y update

RUN apt-get install -y \
                    git \
                    lsb-core \
                    sudo \
                    wget \
                    iproute2 \
                    inetutils-ping \
                    apt-utils

RUN cd /app/borderrouter && ./script/bootstrap && ./bootstrap

RUN cd /app/borderrouter && ./configure && make -j8 && make install

RUN cd /app/borderrouter && export NAT64=1 && ./script/setup
