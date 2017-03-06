project(test)

include_directories(../inc)
aux_source_directory(../src SRC_DIR)
aux_source_directory(. SRC_DIR)

add_compile_options(-g)
add_executable(test ${SRC_DIR})
target_link_libraries(test rt)

