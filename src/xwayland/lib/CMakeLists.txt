add_library(KWinXwaylandCommon STATIC
    xwaylandsocket.cpp
    xauthority.cpp
)



target_include_directories(KWinXwaylandCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(KWinXwaylandCommon Qt::Core)
