cmake_minimum_required(VERSION 3.5)

project(rvd)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSOCKLEN_T=socklen_t -g ")

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)

# RKNN lib
set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/3rdparty/librknn_api)
set(RKNN_RT_LIB ${RKNN_API_PATH}/aarch64/librknnrt.so)
include_directories(${RKNN_API_PATH}/include)

# RGA
#set(RGA_PATH ${CMAKE_SOURCE_DIR}/3rdparty/rga/RK3588)
#set(RGA_LIB ${RGA_PATH}/lib/Linux/aarch64/librga.so)
#include_directories(${RGA_PATH}/include)

# RTSP
aux_source_directory(src/xop SOURCE1)
aux_source_directory(src/net SOURCE2)

include_directories(    
    src    
    src/xop
    src/net
    3rdparty
    3rdparty/librknn_api/include
    )

add_executable(${PROJECT_NAME}
src/main.cpp
${SOURCE1}
${SOURCE2}
3rdparty/md5/md5.hpp
)
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS} ${RKNN_RT_LIB} OpenSSL::SSL OpenSSL::Crypto pthread)
