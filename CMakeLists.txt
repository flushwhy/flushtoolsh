cmake_minimum_required(VERSION 3.5.0)
project(flushtools VERSION 0.1.0 LANGUAGES C)

add_library(flushtools INTERFACE)

target_include_directories(flushtools INTERFACE include)

