target_sources(kwin PRIVATE
    qpaintersurfacetexture.cpp
    qpaintersurfacetexture_internal.cpp
    qpaintersurfacetexture_wayland.cpp
    qpainterbackend.cpp
)
target_include_directories(kwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
