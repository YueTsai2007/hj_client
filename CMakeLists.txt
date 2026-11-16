cmake_minimum_required(VERSION 3.16)

project(HJ_Client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(
    ./RM2027-CustomTerminal-CommunicationBackend
    rm2027_backend
)

add_executable(HJ_Client main.cpp)