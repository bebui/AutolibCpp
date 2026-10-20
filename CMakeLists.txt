cmake_minimum_required(VERSION 3.25)

project(autolib
    VERSION 0.1.0
    DESCRIPTION "Finite automata over integer symbols, with fast algorithms and natural operators"
    HOMEPAGE_URL "https://github.com/bebui/AutolibCpp"
    LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(AUTOLIB_TOP_LEVEL ON)
else()
    set(AUTOLIB_TOP_LEVEL OFF)
endif()

option(AUTOLIB_BUILD_TESTS "Build the tests" ${AUTOLIB_TOP_LEVEL})
option(AUTOLIB_BUILD_EXAMPLES "Build the examples" ${AUTOLIB_TOP_LEVEL})
option(AUTOLIB_BUILD_BENCHMARKS "Build the benchmarks" OFF)
option(AUTOLIB_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(AUTOLIB_SANITIZE "Build with the address and undefined behaviour sanitizers" OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

configure_file(include/autolib/version.hpp.in "${PROJECT_BINARY_DIR}/include/autolib/version.hpp" @ONLY)

add_library(autolib
    src/automaton.cpp
    src/big_integer.cpp
    src/compiled_dfa.cpp
    src/determinize.cpp
    src/dyadic.cpp
    src/interval_set.cpp
    src/languages.cpp
    src/minimize.cpp
    src/reduce.cpp
    src/regex.cpp
    src/searcher.cpp
    src/weighted/compiled.cpp
    src/weighted/enumeration.cpp
    src/weighted/exact_costs.cpp
    src/weighted/graph.cpp
    src/weighted/layered.cpp
    src/weighted/multi.cpp
    src/weighted/paths.cpp
    src/weighted/transformations.cpp
    src/weighted/weighted_automaton.cpp
    src/words.cpp)
add_library(autolib::autolib ALIAS autolib)

target_compile_features(autolib PUBLIC cxx_std_20)
set_target_properties(autolib PROPERTIES
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME autolib)
target_include_directories(autolib
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(autolib PRIVATE AUTOLIB_BUILDING)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(autolib PUBLIC AUTOLIB_SHARED)
endif()

# Warnings and sanitizers, shared by every target of the project
add_library(autolib_options INTERFACE)
if(MSVC)
    target_compile_options(autolib_options INTERFACE /W4 /permissive- $<$<BOOL:${AUTOLIB_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(autolib_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        $<$<BOOL:${AUTOLIB_WARNINGS_AS_ERRORS}>:-Werror>)
    if(AUTOLIB_SANITIZE)
        target_compile_options(autolib_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(autolib_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()
target_link_libraries(autolib PRIVATE $<BUILD_INTERFACE:autolib_options>)
if(AUTOLIB_SANITIZE AND NOT MSVC)
    target_link_options(autolib PUBLIC $<BUILD_INTERFACE:-fsanitize=address,undefined>)
endif()

if(AUTOLIB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(AUTOLIB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(AUTOLIB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Installation : find_package(autolib) then target_link_libraries(... autolib::autolib)
install(TARGETS autolib EXPORT autolibTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/autolib DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")
install(FILES "${PROJECT_BINARY_DIR}/include/autolib/version.hpp" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/autolib)
install(EXPORT autolibTargets NAMESPACE autolib:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/autolib)
configure_package_config_file(cmake/autolibConfig.cmake.in "${PROJECT_BINARY_DIR}/autolibConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/autolib)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/autolibConfigVersion.cmake" COMPATIBILITY SameMajorVersion)
install(FILES "${PROJECT_BINARY_DIR}/autolibConfig.cmake" "${PROJECT_BINARY_DIR}/autolibConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/autolib)
