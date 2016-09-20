cmake_minimum_required(VERSION 2.8.4)
project(qemu)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_custom_target(qemu COMMAND make -C ${qemu_SOURCE_DIR}
        CLION_EXE_DIR=${PROJECT_BINARY_DIR})