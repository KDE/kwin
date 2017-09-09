add_subdirectory(qpainter)
add_subdirectory(opengl)

if (Vulkan_FOUND)
    add_subdirectory(vulkan)
endif()
