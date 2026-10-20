# The Python module autolib._autolib, built by pip through scikit-build-core.

find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module OPTIONAL_COMPONENTS Development.SABIModule)
execute_process(COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(_autolib STABLE_ABI NB_STATIC LTO
    src/module.cpp
    src/compiled.cpp
    src/core.cpp
    src/weighted.cpp)
target_link_libraries(_autolib PRIVATE autolib::autolib)
target_compile_features(_autolib PRIVATE cxx_std_20)

# Type stubs, for completion and type checkers
nanobind_add_stub(_autolib_stub MODULE _autolib OUTPUT _autolib.pyi PYTHON_PATH $<TARGET_FILE_DIR:_autolib> DEPENDS _autolib)

install(TARGETS _autolib LIBRARY DESTINATION autolib)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/_autolib.pyi" DESTINATION autolib)
