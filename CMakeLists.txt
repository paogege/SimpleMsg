cmake_minimum_required (VERSION 3.8)
project(SFD)
include_directories(${CMAKE_SOURCE_DIR}/SimpleMsg)
add_subdirectory("SimpleMsg")
add_executable (Svr "Svr.cpp")
add_executable (Cln "Cln.cpp")

target_link_libraries(Svr SimpleMsg)
target_link_libraries(Cln SimpleMsg)
