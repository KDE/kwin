target_sources(kwin PRIVATE
    softwarevsyncmonitor.cpp softwarevsyncmonitor.h
    vsyncmonitor.cpp vsyncmonitor.h
)

target_include_directories(kwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
