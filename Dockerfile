FROM ubuntu

WORKDIR /app/borderrouter

ADD . /app/borderrouter

RUN apt-get -y update

RUN apt-get install -y \
                    lsb-core \
                    sudo \
                    wget \
                    iproute2 \
                    inetutils-ping \
                    apt-utils

RUN cd /app/borderrouter && ./script/bootstrap && ./bootstrap

RUN cd /app/borderrouter && ./configure --with-dbusconfdir="/etc/" && make -j8 && make install

RUN cd /app/borderrouter && export NAT64=1 && ./script/setup

ENTRYPOINT ["/app/borderrouter/script/docker_entrypoint.sh"]

CMD ["top"]
