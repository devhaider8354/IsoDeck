cmake_minimum_required(VERSION 3.20)
project(IsoDeck VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # static CRT -> no DLLs

# Embedded boot engine: third_party/ventoy.zip  (the official ventoy-x.y.z-windows.zip, renamed)
set(VENTOY_ZIP "${CMAKE_SOURCE_DIR}/third_party/ventoy.zip")
if(EXISTS "${VENTOY_ZIP}")
    message(STATUS "Embedding boot engine: ${VENTOY_ZIP}")
    set(HAVE_VENTOY ON)
else()
    message(WARNING "third_party/ventoy.zip not found - the exe will build WITHOUT the boot engine. See README.md")
    set(HAVE_VENTOY OFF)
endif()

set(SRC
    src/main.cpp src/app.cpp src/app.h
    src/ui.cpp src/ui.h src/gfx.cpp src/gfx.h
    src/disk.cpp src/disk.h src/ventoy.cpp src/ventoy.h
    src/jobs.cpp src/jobs.h src/theme.cpp src/theme.h
    src/i18n.cpp src/i18n.h src/util.cpp src/util.h src/resource.h
    res/app.rc)

add_executable(IsoDeck WIN32 ${SRC})
target_compile_definitions(IsoDeck PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
if(HAVE_VENTOY)
    set_source_files_properties(res/app.rc PROPERTIES COMPILE_DEFINITIONS ISODECK_HAVE_VENTOY)
    set_source_files_properties(res/app.rc PROPERTIES OBJECT_DEPENDS "${VENTOY_ZIP}")
endif()
if(MSVC)
    target_compile_options(IsoDeck PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus $<$<CONFIG:Release>:/O2;/GL;/Gy>)
    target_link_options(IsoDeck PRIVATE /MANIFEST:NO $<$<CONFIG:Release>:/LTCG;/OPT:REF;/OPT:ICF>)
else()
    target_compile_options(IsoDeck PRIVATE -municode -Wall -Wno-unknown-pragmas -Wno-cast-function-type -Wno-unused-parameter)
    target_link_options(IsoDeck PRIVATE -municode -static -mwindows)
endif()
target_link_libraries(IsoDeck PRIVATE d2d1 dwrite windowscodecs dwmapi setupapi winhttp bcrypt shlwapi comdlg32 shell32 ole32 uuid advapi32 user32 gdi32)
set_target_properties(IsoDeck PROPERTIES OUTPUT_NAME "IsoDeck")

install(TARGETS IsoDeck RUNTIME DESTINATION .)
