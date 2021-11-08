target_sources(kwin PRIVATE
    abstract_egl_backend.cpp
    basiceglsurfacetexture_internal.cpp
    basiceglsurfacetexture_wayland.cpp
    egl_dmabuf.cpp
    openglbackend.cpp
    openglsurfacetexture.cpp
    openglsurfacetexture_internal.cpp
    openglsurfacetexture_wayland.cpp
    openglsurfacetexture_x11.cpp
)
target_include_directories(kwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
