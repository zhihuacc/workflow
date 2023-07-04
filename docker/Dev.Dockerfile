FROM docker.io/ubuntu:20.04

RUN apt-get update && apt-get install -y vim cmake g++ gdb  # utlities for dev and debug
RUN apt-get install -y libssl-dev

WORKDIR /workflow

# RUN mkdir build && cmake -B build && cmake --build build && cmake --install build 
#     && cd tutorial && mkdir build && cmake -B build && cmake --build build