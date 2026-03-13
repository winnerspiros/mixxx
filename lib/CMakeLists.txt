# ReplayGain
add_library(replaygain STATIC replaygain/replaygain.cpp)
target_include_directories(replaygain PUBLIC .)

# QM-DSP
file(GLOB_RECURSE QMDSP_SOURCES qm-dsp/*.cpp qm-dsp/*.c)
add_library(qmdsp STATIC ${QMDSP_SOURCES})
target_include_directories(qmdsp PUBLIC qm-dsp)
# Mixxx uses double precision for QM-DSP
target_compile_definitions(qmdsp PUBLIC kiss_fft_scalar=double)
if(NOT MSVC)
    target_compile_options(qmdsp PRIVATE -Wno-float-conversion -Wno-sign-compare)
endif()
