FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    entr \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app