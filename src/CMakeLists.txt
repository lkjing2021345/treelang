add_library(treelang_core INTERFACE)

target_include_directories(treelang_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(model)
add_subdirectory(combat)
add_subdirectory(world)

add_executable(treelang
    main.cpp
)

target_link_libraries(treelang PRIVATE treelang_core pjh_result pjh_json pjh_cli pjh_platform)
