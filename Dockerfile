FROM debian:latest

RUN apt-get update -y && apt-get install -y gcc build-essential zlib1g-dev \
    libssl-dev libevent-dev pkg-config postgresql-client libpq-dev \
    mariadb-client libmariadb-dev-compat libpcre3-dev \
    icu-devtools

ADD . /src

WORKDIR /src

RUN ./configure && make && make install && make portmsg
