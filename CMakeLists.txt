cmake_minimum_required(VERSION 3.21)

project(anim_view LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(anim_view src/main.cpp
                         src/AnimatedModel.cpp
                         src/AnimatedModel.h
                         src/AssetLoader.cpp
                         src/AssetLoader.h
                         src/Camera.h
                         src/Light.h
                         src/Material.h
                         src/Shader.cpp
                         src/Shader.h
                         src/stb_image.cpp
                         src/Texture.cpp
                         src/Texture.h
)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

find_path(STB_INCLUDE_DIRS "stb.h")

find_package(imgui CONFIG REQUIRED)

target_include_directories(anim_view PRIVATE ${STB_INCLUDE_DIRS})
target_link_libraries(anim_view PRIVATE glfw
                                        glad::glad
                                        glm::glm
                                        imgui::imgui
)

if(MSVC)
  target_compile_options(anim_view PRIVATE /W4)
else()
  target_compile_options(anim_view PRIVATE -Wall -Wextra -pedantic)
endif()

if (WIN32)
    add_custom_command(TARGET anim_view POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_directory
                       "${CMAKE_SOURCE_DIR}/data"
                       $<TARGET_FILE_DIR:anim_view>
    )
endif()