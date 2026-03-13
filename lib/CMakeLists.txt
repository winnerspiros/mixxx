# ReplayGain
add_library(replaygain STATIC replaygain/replaygain.cpp)
target_include_directories(replaygain PUBLIC .)

# QM-DSP
file(GLOB_RECURSE QMDSP_SOURCES qm-dsp/*.cpp)
add_library(qmdsp STATIC ${QMDSP_SOURCES})
target_include_directories(qmdsp PUBLIC qm-dsp)
if(NOT MSVC)
    target_compile_options(qmdsp PRIVATE -Wno-float-conversion -Wno-sign-compare)
endif()
