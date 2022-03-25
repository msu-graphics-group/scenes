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


set(HYDRA_API_FOLDER "/home/frol/PROG/HydraRepos/HydraAPI")

include_directories(${CMAKE_SOURCE_DIR}/external)
include_directories(${CMAKE_SOURCE_DIR}/external/embree)
include_directories(${HYDRA_API_FOLDER}/hydra_api)

find_package(OpenMP)

set(SCENE_LOADER_SRC
        ${CMAKE_SOURCE_DIR}/loader/pugixml.cpp
        ${CMAKE_SOURCE_DIR}/loader/hydraxml.cpp
        ${CMAKE_SOURCE_DIR}/loader/cmesh.cpp)

set(RENDER_SRC
        ${CMAKE_SOURCE_DIR}/render/EmbreeRT.cpp
        ${CMAKE_SOURCE_DIR}/render/image_save.cpp
        ${CMAKE_SOURCE_DIR}/render/raytracing.cpp
        ${CMAKE_SOURCE_DIR}/CamHostRaysPinHoleBSPImage.cpp
        ${CMAKE_SOURCE_DIR}/main.cpp)

set(HYDRA_SRC HydraRngUtils.cpp)

#add_library(hydra_bsp_image SHARED svm.cpp BSP_based_sampler.cpp ${SCENE_LOADER_SRC} ${RENDER_SRC} ${HYDRA_SRC})	
add_executable(hydra_bsp_image main.cpp svm.cpp BSP_based_sampler.cpp ${SCENE_LOADER_SRC} ${RENDER_SRC} ${HYDRA_SRC})

set_property(TARGET hydra_bsp_image PROPERTY CXX_STANDARD 17)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set_target_properties(hydra_bsp_image PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

    target_link_libraries(hydra_bsp_image PRIVATE project_warnings ${RAYTRACING_EMBREE_LIBS})

    add_custom_command(TARGET hydra_bsp_image POST_BUILD COMMAND ${CMAKE_COMMAND}
            -E copy_directory "${PROJECT_SOURCE_DIR}/external/embree/bin_win64" $<TARGET_FILE_DIR:hydra_bsp_image>)
else()
    target_link_libraries(hydra_bsp_image PRIVATE project_warnings Threads::Threads dl ${RAYTRACING_EMBREE_LIBS})
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(hydra_bsp_image PUBLIC OpenMP::OpenMP_CXX)
endif()