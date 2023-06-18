FROM docker.io/ubuntu:20.04

RUN apt-get update && apt-get install -y vim cmake g++ libssl-dev

WORKDIR /workflow

# RUN mkdir build && cmake -B build && cmake --build build && cmake --install build 
#     && cd tutorial && mkdir build && cmake -B build && cmake --build build