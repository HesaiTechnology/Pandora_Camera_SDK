cmake_minimum_required(VERSION 3.5)

project(pandora_client)

#add_executable(pandora_client_test
#	src/main.c
#	src/pandora_client.c
#)
#
#target_link_libraries(pandora_client_test
#	pandora_client
#	pthread
#)
#
#include_directories(
#	include
#)
#
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g" )




add_library(${PROJECT_NAME} src/pandora_client.c)

include_directories(
	include
)