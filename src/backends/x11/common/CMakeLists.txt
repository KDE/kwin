if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-int-to-pointer-cast")
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-int-to-void-pointer-cast")
endif()
target_sources(kwin PRIVATE
    kwinxrenderutils.cpp
    x11_common_egl_backend.cpp
    x11_common_logging.cpp
)
