target_sources(kwin PRIVATE
    qpaintersurfacetexture.cpp qpaintersurfacetexture.h
    qpaintersurfacetexture_internal.cpp qpaintersurfacetexture_internal.h
    qpaintersurfacetexture_wayland.cpp qpaintersurfacetexture_wayland.h
    qpainterbackend.cpp qpainterbackend.h
)
target_include_directories(kwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
