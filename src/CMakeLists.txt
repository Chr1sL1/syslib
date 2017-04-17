project(syslib)
enable_language(C ASM)

include_directories(../inc)
set(lib_src_list asm_run_task.s asm_yield_task.s asm_resume_task.s)
aux_source_directory(. lib_src_list)

add_compile_options(-g)

add_library(syslib STATIC ${lib_src_list})
