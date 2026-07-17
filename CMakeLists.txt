cmake_minimum_required(VERSION 3.10)

project(vulkan2026 C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

add_executable(dummy src/main.c)

target_compile_options(dummy PRIVATE
    # -Wall
    # -Wextra
    # -pedantic
)

target_link_libraries(dummy PRIVATE Vulkan::Vulkan)

target_include_directories(dummy PRIVATE
	"${CMAKE_SOURCE_DIR}/inc/"
	"${CMAKE_SOURCE_DIR}/../SDL/include/"
	"${CMAKE_SOURCE_DIR}/../glm/"
	"${CMAKE_SOURCE_DIR}/../volk/"
	"${CMAKE_SOURCE_DIR}/../VulkanMemoryAllocator/include/"
	"${CMAKE_SOURCE_DIR}/../cglm/include"
)


target_compile_definitions(dummy PRIVATE DEBUG LOG_LVL=3)
