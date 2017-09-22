cmake_minimum_required(VERSION 3.5)

#add_executable(pandora_client
#	src/main.c
#	src/pandora_client.c
#)
#
#target_link_libraries(pandora_client
#	pthread
#)
#
#include_directories(
#	include
#)
#
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g" )
#
#install(PROGRAMS ${CMAKE_BINARY_DIR}/pandar_control DESTINATION bin)

project(pandora_client)

add_library(${PROJECT_NAME} src/pandora_client.c)

include_directories(
	include
)