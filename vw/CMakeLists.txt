file(GLOB VW_HEADERS include/*.h include/*.hpp)
source_group("headers" FILES ${VW_HEADERS})
file(GLOB VW_SRC lib_src/*.cpp)

add_library(libVW STATIC ${VW_HEADERS} ${VW_SRC})
target_link_libraries(libVW ${VULKAN_LIBRARY} ${GLFW_LIBRARY} ${SHADERC_LIBRARY})