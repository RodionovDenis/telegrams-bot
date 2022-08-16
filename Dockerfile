FROM alpine:latest

LABEL maintainer Bill Wang <ozbillwang@gmail.com>

RUN apk update && apk upgrade
RUN apk --update add \
        g++ \
        cmake \
        make \
        libstdc++  \
        poco-dev \
        openssl-dev \
    rm -rf /var/cache/apk/*
COPY . ./push-up-bot

WORKDIR push-up-bot
RUN mkdir build
WORKDIR build
RUN cmake -DCMAKE_BUILD_TYPE=Release ..
RUN make -j4

CMD ["./PushUpBot"]

