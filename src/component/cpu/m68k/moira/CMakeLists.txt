# "Moiris-4.0"

add_library(moira STATIC

Moira.cpp
)

target_compile_options(moira PUBLIC -Wno-unused-parameter)
target_compile_options(moira PUBLIC -Wno-unused-but-set-parameter)
target_compile_options(moira PUBLIC -Wno-unused-but-set-variable)
target_compile_options(moira PUBLIC -Wno-missing-field-initializers)

if(MINGW)
    target_compile_options(moira PUBLIC -Wa,-mbig-obj)
endif()
