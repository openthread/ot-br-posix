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
                    apt-utils \
                    bind9

RUN cd /app/borderrouter && ./script/bootstrap && ./bootstrap

RUN cd /app/borderrouter && ./configure --with-dbusconfdir="/etc/" && make -j8 && make install

RUN cd /app/borderrouter && NAT64=1 ./script/setup

RUN mv /app/borderrouter/script/docker_dns64.conf /etc/bind/named.conf.options

RUN chmod 644 /etc/bind/named.conf.options

ENTRYPOINT ["/app/borderrouter/script/docker_entrypoint.sh"]
