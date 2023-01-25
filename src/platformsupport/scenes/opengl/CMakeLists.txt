target_sources(kwin PRIVATE
    abstract_egl_backend.cpp abstract_egl_backend.h
    basiceglsurfacetexture_internal.cpp basiceglsurfacetexture_internal.h
    basiceglsurfacetexture_wayland.cpp basiceglsurfacetexture_wayland.h
    egl_dmabuf.cpp egl_dmabuf.h
    openglbackend.cpp openglbackend.h
    openglsurfacetexture.cpp openglsurfacetexture.h
    openglsurfacetexture_internal.cpp openglsurfacetexture_internal.h
    openglsurfacetexture_wayland.cpp openglsurfacetexture_wayland.h
    openglsurfacetexture_x11.cpp openglsurfacetexture_x11.h
)
target_include_directories(kwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
