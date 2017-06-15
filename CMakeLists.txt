project(test)

#enable_language(C ASM)
include_directories(inc)
#set(src_list, src/asm_run_task.s src/asm_yield_task.s src/asm_resume_task.s)
#aux_source_directory(src src_list)
aux_source_directory(test src_list)
add_subdirectory(src)

#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=gnu11 -Wint-to-pointer-cast -Wpointer-to-int-cast")

#add_compile_options(-g)
add_compile_options(-O2)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -finline-functions")

add_executable(test ${src_list})
target_link_libraries(test syslib)
target_link_libraries(test rt)

