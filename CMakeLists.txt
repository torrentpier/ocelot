cmake_minimum_required(VERSION 2.8.4)
project(Ocelot_1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    config.cpp
    config.h
    db.cpp
    db.h
    events.cpp
    events.h
    misc_functions.cpp
    misc_functions.h
    ocelot.cpp
    ocelot.h
    report.cpp
    report.h
    response.cpp
    response.h
    schedule.cpp
    schedule.h
    site_comm.cpp
    site_comm.h
    user.cpp
    user.h
    worker.cpp
    worker.h)

add_executable(Ocelot_1 ${SOURCE_FILES})