project(syslib)
enable_language(C ASM)

include_directories(../inc)
set(lib_src_list asm_run_task.s asm_yield_task.s asm_resume_task.s asm_misc.s)
aux_source_directory(. lib_src_list)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -Wint-to-pointer-cast -Wpointer-to-int-cast")

add_compile_options(-g)
#add_compile_options(-O1)


add_library(syslib STATIC ${lib_src_list})

set(CMAKE_INSTALL_PREFIX ../lib)
install(TARGETS syslib ARCHIVE DESTINATION lib)
