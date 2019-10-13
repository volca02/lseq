cmake_minimum_required(VERSION 3.15)
project(launchpad)

find_package(PkgConfig)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pkg_check_modules(jack REQUIRED IMPORTED_TARGET jack)

add_executable(launchpad
    src/main.cc
    src/lseq.h
    src/ui.cc
    src/ui.h
    src/sequence.cc
    src/sequence.h
    src/event.h
    src/sequencer.h
    src/project.h
    src/router.h
)

target_link_libraries(launchpad PkgConfig::jack)
set_target_properties(launchpad PROPERTIES CXX_STANDARD 17)
