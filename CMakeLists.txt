cmake_minimum_required(VERSION 3.10)
project(socket_demo)

add_executable(server server.cpp)
add_executable(client client.cpp)
add_executable(selectclient selectclient.cpp)
add_executable(tcpselect tcpselect.cpp)

