cmake_minimum_required(VERSION 3.15)
project(Server_Side_HTTP)

set(CMAKE_CXX_STANDARD 14)

add_executable(Server_Side_HTTP main.cpp Classes/Server_Class.cpp Classes/Server_Class.h General/UTILS.h)
SET(CMAKE_CXX_FLAGS -pthread)
