add_executable(quickstart quickstart.cpp)
target_link_libraries(quickstart PRIVATE autolib::autolib autolib_options)
