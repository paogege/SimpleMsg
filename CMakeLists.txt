cmake_minimum_required (VERSION 3.8)
project(SFD)
add_executable (Svr "Svr.cpp" "SimpleMsg.cpp" "SimpleMsg.h")
add_executable (Cln "Cln.cpp" "SimpleMsg.cpp" "SimpleMsg.h")
