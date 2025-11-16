cmake_minimum_required(VERSION 3.20)
project(OpenGLScenery LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SOURCES
    src/main.cpp
    src/App.cpp
    src/GlfwContext.cpp
    src/Timer.cpp
    src/Window.cpp
    src/Camera.cpp
    src/Scene.cpp
    src/DemoSceneBuilder.cpp
    src/SceneRenderer.cpp
    src/SkyboxRenderer.cpp
    src/Shader.cpp
    src/FileSystem.cpp
    third_party/tinyexr/miniz.c
    lib/glad/src/glad.c
)

add_executable(${PROJECT_NAME} ${SOURCES})
set_source_files_properties(third_party/tinyexr/miniz.c PROPERTIES LANGUAGE C)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/glfw-3.4.bin.WIN64/include
    ${CMAKE_SOURCE_DIR}/lib/glad/include
    ${CMAKE_SOURCE_DIR}/lib/glm
    ${CMAKE_SOURCE_DIR}/third_party/tinyobjloader
    ${CMAKE_SOURCE_DIR}/third_party/tinyexr
    ${CMAKE_SOURCE_DIR}/lib/assimp-src/contrib
)

if(WIN32)
    set(GLFW_LIB_DIR ${CMAKE_SOURCE_DIR}/lib/glfw-3.4.bin.WIN64/lib-vc2022)

    target_link_libraries(${PROJECT_NAME}
        ${GLFW_LIB_DIR}/glfw3.lib
        opengl32.lib
        ole32.lib
        windowscodecs.lib
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLFW_LIB_DIR}/glfw3.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/models"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models"
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)