include(FetchContent)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.0)

set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest)

add_executable(treelang_tests test_main.cpp test_smoke.cpp)

target_link_libraries(treelang_tests PRIVATE treelang_core doctest)

target_compile_definitions(treelang_tests PRIVATE TREELANG_BUILD_TESTS)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(treelang_tests)

add_executable(core_matrix_tests test_main.cpp core/matrix_test.cpp)

target_link_libraries(core_matrix_tests PRIVATE treelang_core doctest)

doctest_discover_tests(core_matrix_tests)
