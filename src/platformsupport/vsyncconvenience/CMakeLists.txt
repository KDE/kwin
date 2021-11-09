target_sources(kwin PRIVATE
    softwarevsyncmonitor.cpp
    vsyncmonitor.cpp
)

target_include_directories(kwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
