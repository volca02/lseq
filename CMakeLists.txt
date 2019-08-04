cmake_minimum_required(VERSION 3.15)
project(launchpad)

find_package(PkgConfig)

pkg_check_modules(rtmidi REQUIRED IMPORTED_TARGET rtmidi)

add_executable(launchpad
    main.cc
    lseq.h
    ui.cc
    ui.h
    sequence.cc
    sequence.h
    event.h)

target_link_libraries(launchpad PkgConfig::rtmidi)
set_target_properties(launchpad PROPERTIES CXX_STANDARD 17)
