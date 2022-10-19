add_subdirectory(lib)

add_library(KWinXwaylandServerModule OBJECT
    clipboard.cpp
    databridge.cpp
    datasource.cpp
    dnd.cpp
    drag.cpp
    drag_wl.cpp
    drag_x.cpp
    primary.cpp
    selection.cpp
    selection_source.cpp
    transfer.cpp
    xwayland.cpp
    xwldrophandler.cpp
    xwaylandlauncher.cpp
)
set_property(TARGET KWinXwaylandServerModule PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(KWinXwaylandServerModule PUBLIC kwin KWinXwaylandCommon)
