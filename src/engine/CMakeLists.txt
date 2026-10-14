add_library(clivcut_engine STATIC
    DecoderFrameSource.h DecoderFrameSource.cpp
    AudioMixer.h AudioMixer.cpp
    Exporter.h Exporter.cpp
    Transcriber.h Transcriber.cpp
    WhisperModels.h WhisperModels.cpp
    Proxy.h Proxy.cpp
)
target_include_directories(clivcut_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(clivcut_engine PUBLIC clivcut_core clivcut_media clivcut_render)
if(TARGET whisper)
    target_link_libraries(clivcut_engine PRIVATE whisper)
    target_compile_definitions(clivcut_engine PRIVATE CLIVCUT_HAVE_WHISPER)
endif()
