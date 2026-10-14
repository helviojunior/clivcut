add_library(clivcut_media STATIC
    FFmpegUtil.h FFmpegUtil.cpp
    MediaProbe.h MediaProbe.cpp
    VideoDecoder.h VideoDecoder.cpp
    AudioDecoder.h AudioDecoder.cpp
    Encoder.h Encoder.cpp
    Loudness.h Loudness.cpp
)
target_include_directories(clivcut_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(clivcut_media PUBLIC clivcut_core Qt6::Gui clivcut::ffmpeg)
