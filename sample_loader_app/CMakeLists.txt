cmake_minimum_required(VERSION 3.15)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

project(loader_app CXX)

add_library(project_warnings INTERFACE)
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)


if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    link_directories(${CMAKE_SOURCE_DIR}/external/embree/lib_win64)
    set(RAYTRACING_EMBREE_LIBS
            embree3)
else()
    find_package(Threads REQUIRED)
    link_directories(${CMAKE_SOURCE_DIR}/external/embree/lib)
    set(RAYTRACING_EMBREE_LIBS
            embree3 embree_sse42 embree_avx embree_avx2 lexers simd sys tasking)
endif()

include_directories(${CMAKE_SOURCE_DIR}/external)
include_directories(${CMAKE_SOURCE_DIR}/external/embree)

find_package(OpenMP)

set(SCENE_LOADER_SRC
        ${CMAKE_SOURCE_DIR}/loader/pugixml.cpp
        ${CMAKE_SOURCE_DIR}/loader/hydraxml.cpp
        ${CMAKE_SOURCE_DIR}/loader/cmesh.cpp)

set(RENDER_SRC
        ${CMAKE_SOURCE_DIR}/render/EmbreeRT.cpp
        ${CMAKE_SOURCE_DIR}/render/image_save.cpp
        ${CMAKE_SOURCE_DIR}/render/raytracing.cpp)

add_executable(loader_app main.cpp svm.cpp ${SCENE_LOADER_SRC} ${RENDER_SRC})

set_property(TARGET loader_app PROPERTY CXX_STANDARD 17)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set_target_properties(loader_app PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

    target_link_libraries(loader_app PRIVATE project_warnings ${RAYTRACING_EMBREE_LIBS})

    add_custom_command(TARGET loader_app POST_BUILD COMMAND ${CMAKE_COMMAND}
            -E copy_directory "${PROJECT_SOURCE_DIR}/external/embree/bin_win64" $<TARGET_FILE_DIR:loader_app>)
else()
    target_link_libraries(loader_app PRIVATE project_warnings Threads::Threads dl ${RAYTRACING_EMBREE_LIBS})
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(loader_app PUBLIC OpenMP::OpenMP_CXX)
endif()