project(test)

include_directories(../inc)
aux_source_directory(../src SRC_DIR)
aux_source_directory(. SRC_DIR)

set(CMAKE_EXPORT_COMPILE_COMMAND ON)
add_compile_options(-O2)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -finline-functions")

add_executable(test ${SRC_DIR})
target_link_libraries(test rt)

