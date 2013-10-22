cmake_minimum_required(VERSION 2.8)
project(nuclear)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -g -std=c++0x")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(Wayland)
find_package(PkgConfig "0.22" REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)
add_subdirectory(protocol)

# uninstall target
configure_file("${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

configure_file("${CMAKE_SOURCE_DIR}/nuclear.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/nuclear.pc" IMMEDIATE @ONLY)
add_custom_target(pkgconfig COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/nuclear.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nuclear.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
