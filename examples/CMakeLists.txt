add_executable(quickstart quickstart.cpp)
target_link_libraries(quickstart PRIVATE autolib::autolib autolib_options)
add_executable(weighted weighted.cpp)
target_link_libraries(weighted PRIVATE autolib::autolib autolib_options)
