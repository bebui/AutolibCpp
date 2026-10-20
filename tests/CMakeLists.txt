include(FetchContent)
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
include(Catch)

add_executable(autolib_tests
    automaton_test.cpp
    big_integer_test.cpp
    interval_set_test.cpp
    java_oracle_test.cpp
    language_test.cpp
    random_test.cpp
    regex_test.cpp
    search_test.cpp
    words_test.cpp)
target_link_libraries(autolib_tests PRIVATE autolib::autolib autolib_options Catch2::Catch2WithMain)
target_compile_definitions(autolib_tests PRIVATE AUTOLIB_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
catch_discover_tests(autolib_tests)
