add_definitions(-DSHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/")

file(GLOB SHADERS shaders)
source_group("shaders" FILES ${SHADERS})

add_executable(example example.cpp ${SHADERS})
target_link_libraries(example libVW ${VULKAN_LIBRARY} ${GLFW_LIBRARY} ${SHADERC_LIBRARY})