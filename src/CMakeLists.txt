project(syslib)
enable_language(C ASM)

include_directories(../inc)
set(lib_src_list asm_run_task.s asm_yield_task.s asm_resume_task.s misc.s)
aux_source_directory(. lib_src_list)

#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=gnu11 -Wint-to-pointer-cast -Wpointer-to-int-cast")

add_compile_options(-g)
add_compile_options(-O2)


add_library(syslib STATIC ${lib_src_list})
