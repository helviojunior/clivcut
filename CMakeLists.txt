cmake_minimum_required(VERSION 3.24)

project(clivcut
    VERSION 0.1.0
    DESCRIPTION "Editor de vídeo multiplataforma open-source"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

set(QT_NO_PRIVATE_MODULE_WARNING ON)  # RHI é API semi-pública do Qt

option(CLIVCUT_BUILD_TESTS "Compilar testes" ON)
option(CLIVCUT_BUILD_APP "Compilar a interface gráfica" ON)
option(CLIVCUT_WITH_WHISPER "Legendas automáticas locais com whisper.cpp" ON)
option(CLIVCUT_WHISPER_VULKAN "Aceleração Vulkan no whisper.cpp (compilação mais lenta)" OFF)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui GuiPrivate ShaderTools)
if(CLIVCUT_BUILD_APP)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Quick QuickControls2 Concurrent Network)
endif()
qt_standard_project_setup(REQUIRES 6.7)

# FFmpeg e SDL2: pkg-config (Linux, Homebrew, MSYS2) ou pacotes CMake (vcpkg).
find_package(PkgConfig QUIET)
add_library(clivcut_ffmpeg INTERFACE)
add_library(clivcut::ffmpeg ALIAS clivcut_ffmpeg)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET
        libavformat libavcodec libavutil libswscale libswresample libavfilter)
endif()
if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(clivcut_ffmpeg INTERFACE PkgConfig::FFMPEG)
else()
    find_package(FFMPEG REQUIRED) # FindFFMPEG do vcpkg
    target_include_directories(clivcut_ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(clivcut_ffmpeg INTERFACE ${FFMPEG_LIBRARY_DIRS})
    target_link_libraries(clivcut_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})
endif()

if(CLIVCUT_BUILD_APP)
    add_library(clivcut_sdl2 INTERFACE)
    add_library(clivcut::sdl2 ALIAS clivcut_sdl2)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
    endif()
    if(TARGET PkgConfig::SDL2)
        target_link_libraries(clivcut_sdl2 INTERFACE PkgConfig::SDL2)
    else()
        find_package(SDL2 CONFIG REQUIRED)
        target_link_libraries(clivcut_sdl2 INTERFACE SDL2::SDL2)
    endif()
endif()

if(CLIVCUT_WITH_WHISPER)
    # Usa o whisper.cpp do sistema se existir; senão compila uma versão fixa, estática.
    find_package(whisper CONFIG QUIET)
    if(NOT TARGET whisper)
        include(FetchContent)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # estático: binário autocontido
        set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
        set(GGML_VULKAN ${CLIVCUT_WHISPER_VULKAN} CACHE BOOL "" FORCE)
        FetchContent_Declare(whisper
            GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
            GIT_TAG v1.9.4
            GIT_SHALLOW TRUE
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(whisper)
        set(CLIVCUT_WHISPER_SAMPLES ${whisper_SOURCE_DIR}/samples)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W3 /utf-8 /permissive- /Zc:__cplusplus)
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES)
endif()

add_subdirectory(src/core)
add_subdirectory(src/media)
add_subdirectory(src/render)
add_subdirectory(src/engine)
add_subdirectory(src/cli)

if(CLIVCUT_BUILD_APP)
    add_subdirectory(src/app)
endif()

# Instaladores: NSIS (Windows), DMG (macOS), TGZ (outros).
set(CPACK_PACKAGE_NAME "clivcut")
set(CPACK_PACKAGE_VENDOR "clivcut")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Editor de vídeo multiplataforma open-source")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "clivcut")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_EXECUTABLES "clivcut" "clivcut")
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/packaging/windows/clivcut.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/clivcut.exe")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\$STARTMENU_FOLDER\\clivcut.lnk' '$INSTDIR\\bin\\clivcut.exe'")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)

if(CLIVCUT_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
