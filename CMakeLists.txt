cmake_minimum_required(VERSION 3.5)
project(pandora_client)

add_library(${PROJECT_NAME} src/pandora_client.c)

include_directories(
	include
)