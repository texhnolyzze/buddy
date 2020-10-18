cmake_minimum_required(VERSION 3.17)
project(buddy C)

set(CMAKE_C_STANDARD 99)

add_executable(buddy test.c buddy.h buddy.c)