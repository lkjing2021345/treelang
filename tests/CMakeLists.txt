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

add_executable(core_tests test_main.cpp core/core_test.cpp)

target_link_libraries(core_tests PRIVATE treelang_core doctest)

doctest_discover_tests(core_tests)

add_executable(model_tests test_main.cpp model/model_test.cpp)

target_link_libraries(model_tests PRIVATE treelang_model doctest)

doctest_discover_tests(model_tests)

add_executable(combat_tests test_main.cpp combat/combat_test.cpp)

target_link_libraries(combat_tests PRIVATE treelang_combat doctest)

doctest_discover_tests(combat_tests)

add_executable(world_tests test_main.cpp world/world_test.cpp)

target_link_libraries(world_tests PRIVATE treelang_world doctest)

doctest_discover_tests(world_tests)

add_executable(achieve_tests test_main.cpp achieve/achieve_test.cpp)

target_link_libraries(achieve_tests PRIVATE treelang_achieve doctest)

doctest_discover_tests(achieve_tests)
