cmake_minimum_required(VERSION 3.19)

project(VidOps
    VERSION 0.1.1
    DESCRIPTION "Cross-platform desktop video downloader"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(VIDOPS_BUILD_TESTS "Build the unit tests" ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)

# --- Core: process orchestration, argument building and parsing (Qt Core only)
add_library(vidops_core STATIC
    src/core/Options.h          src/core/Options.cpp
    src/core/YtDlp.h            src/core/YtDlp.cpp
    src/core/Ffmpeg.h           src/core/Ffmpeg.cpp
    src/core/Tools.h            src/core/Tools.cpp
    src/core/Process.h          src/core/Process.cpp
    src/core/DownloadJob.h      src/core/DownloadJob.cpp
    src/core/DownloadManager.h  src/core/DownloadManager.cpp
    src/core/AppSettings.h      src/core/AppSettings.cpp
)
target_include_directories(vidops_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(vidops_core PUBLIC Qt6::Core)
target_compile_definitions(vidops_core PUBLIC VIDOPS_VERSION="${PROJECT_VERSION}")

# --- GUI
set(VIDOPS_PLATFORM_RESOURCES)
if(WIN32)
    set(VIDOPS_PLATFORM_RESOURCES resources/vidops.rc)
elseif(APPLE)
    set(VIDOPS_PLATFORM_RESOURCES resources/vidops.icns)
    set_source_files_properties(resources/vidops.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

qt_add_executable(VidOps WIN32 MACOSX_BUNDLE
    ${VIDOPS_PLATFORM_RESOURCES}
    resources/vidops.qrc
    src/main.cpp
    src/ui/MainWindow.h         src/ui/MainWindow.cpp
    src/ui/SettingsDialog.h     src/ui/SettingsDialog.cpp
    src/ui/ProgressDelegate.h   src/ui/ProgressDelegate.cpp
)
target_link_libraries(VidOps PRIVATE vidops_core Qt6::Widgets)
set_target_properties(VidOps PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER io.vidops.VidOps
    MACOSX_BUNDLE_BUNDLE_NAME VidOps
    MACOSX_BUNDLE_ICON_FILE vidops.icns
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

if(MSVC)
    target_compile_options(vidops_core PRIVATE /W4 /utf-8)
    target_compile_options(VidOps PRIVATE /W4 /utf-8)
else()
    target_compile_options(vidops_core PRIVATE -Wall -Wextra)
    target_compile_options(VidOps PRIVATE -Wall -Wextra)
endif()

include(GNUInstallDirs)
install(TARGETS VidOps
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
    install(FILES resources/vidops.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/vidops.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()

if(VIDOPS_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    add_executable(tst_core tests/tst_core.cpp)
    target_link_libraries(tst_core PRIVATE vidops_core Qt6::Test)
    add_test(NAME tst_core COMMAND tst_core)
endif()
