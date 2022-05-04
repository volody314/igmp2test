cmake_minimum_required(VERSION 3.5)

project(igmp2test LANGUAGES C)

add_executable(igmp2test main.c)

target_link_libraries(igmp2test -lpthread)
