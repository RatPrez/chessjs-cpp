cmake_minimum_required(VERSION 3.20)
project(chessjs VERSION 1.0.0 DESCRIPTION "A header-only C++17 port of chess.js" LANGUAGES CXX)

# chessjs.hpp: a header-only C++17 port of chess.js (BSD-2-Clause, see LICENSE).

# The examples build when chessjs is the project itself; inside another project (add_subdirectory, FetchContent) only
# the library target is defined, unless this is switched on.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_chessjs_top ON)
else()
    set(_chessjs_top OFF)
endif()
option(CHESSJS_BUILD_EXAMPLES "Build the programs in examples/" ${_chessjs_top})
option(CHESSJS_INSTALL "Generate the install target" ${_chessjs_top})

add_library(chessjs INTERFACE)
add_library(chessjs::chessjs ALIAS chessjs)
target_include_directories(chessjs INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(chessjs INTERFACE cxx_std_17)

if (CHESSJS_BUILD_EXAMPLES)
    add_executable(chessjs_example examples/example.cpp)
    target_link_libraries(chessjs_example PRIVATE chessjs::chessjs)
    if (MSVC)
        target_compile_options(chessjs_example PRIVATE /W4 /permissive-)
    else()
        target_compile_options(chessjs_example PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# find_package(chessjs) after `cmake --install`.
if (CHESSJS_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS chessjs EXPORT chessjsTargets)
    install(FILES include/chessjs.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/chessjs)
    install(EXPORT chessjsTargets
        NAMESPACE chessjs::
        DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/chessjs)

    configure_package_config_file(cmake/chessjsConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/chessjsConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/chessjs)
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/chessjsConfigVersion.cmake
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/chessjsConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/chessjsConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/chessjs)
endif()
