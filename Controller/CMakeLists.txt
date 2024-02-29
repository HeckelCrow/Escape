cmake_minimum_required (VERSION 3.8)

set (proj_name EscapeGame)
project (${proj_name} C CXX)

include_directories(source)
include_directories("../Common")
add_subdirectory(source)
