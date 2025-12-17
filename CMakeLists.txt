cmake_minimum_required(VERSION 3.16)
project(SisterAppEngine VERSION 3.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Eigen3 REQUIRED)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

add_executable(sisterapp
    main.cpp
    src/init_sdl.cpp

    src/renderer.cpp
    src/shader_loader.cpp
    src/imgui_backend.cpp
    src/world.cpp
    src/core/graphics_context.cpp
    src/core/input_manager.cpp
    src/core/input_manager.cpp
    src/core/application.cpp
    src/core/preferences.cpp
    src/core/swapchain.cpp
    src/core/command_pool.cpp
    src/core/sync_objects.cpp
    src/resources/buffer.cpp
    src/graphics/mesh.cpp
    src/graphics/geometry_utils.cpp
    src/math/noise.cpp
    src/math/frustum.cpp
    src/graphics/camera.cpp
    src/graphics/shader.cpp
    src/graphics/material.cpp
    src/graphics/animation.cpp
    src/ui/ui_layer.cpp
    src/ui/minimap.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(sisterapp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
)

target_include_directories(sisterapp SYSTEM PRIVATE
    ${EIGEN3_INCLUDE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

if (TARGET SDL2::SDL2)
    target_link_libraries(sisterapp PRIVATE SDL2::SDL2)
else()
    target_link_libraries(sisterapp PRIVATE ${SDL2_LIBRARIES})
endif()

target_link_libraries(sisterapp PRIVATE Vulkan::Vulkan)

find_program(GLSLC glslc)
set(SHADER_SRC
    shaders/basic.vert
    shaders/basic.frag
    shaders/red.frag
    shaders/environment.vert
    shaders/environment.frag
)
if (GLSLC)
    set(SPIRV_BINARIES "")
    foreach(SHADER ${SHADER_SRC})
        get_filename_component(FILE_NAME ${SHADER} NAME)
        set(OUTPUT_SPV ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${FILE_NAME}.spv)
        add_custom_command(
            OUTPUT ${OUTPUT_SPV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
            COMMAND ${GLSLC} -o ${OUTPUT_SPV} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            COMMENT "Compilando ${SHADER} para SPIR-V"
            VERBATIM
        )
        list(APPEND SPIRV_BINARIES ${OUTPUT_SPV})
    endforeach()
    add_custom_target(shaders_spirv ALL DEPENDS ${SPIRV_BINARIES})
    add_dependencies(sisterapp shaders_spirv)
else()
    message(WARNING "glslc nao encontrado; coloque .spv em shaders/ manualmente.")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(sisterapp PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion")
elseif (MSVC)
    target_compile_options(sisterapp PRIVATE /W4 /permissive-)
endif()

target_sources(sisterapp PRIVATE src/terrain/terrain_map.cpp src/terrain/terrain_generator.cpp src/terrain/terrain_renderer.cpp src/terrain/hydrology_report.cpp src/terrain/watershed.cpp src/terrain/landscape_metrics.cpp)
