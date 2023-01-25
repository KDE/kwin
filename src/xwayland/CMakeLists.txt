add_subdirectory(lib)

add_library(KWinXwaylandServerModule OBJECT
    clipboard.cpp clipboard.h
    databridge.cpp databridge.h
    datasource.cpp datasource.h
    dnd.cpp dnd.h
    drag.cpp drag.h
    drag_wl.cpp drag_wl.h
    drag_x.cpp drag_x.h
    primary.cpp primary.h
    selection.cpp selection.h
    selection_source.cpp selection_source.h
    transfer.cpp transfer.h
    xwayland.cpp xwayland.h
    xwldrophandler.cpp xwldrophandler.h
    xwaylandlauncher.cpp xwaylandlauncher.h
)
set_property(TARGET KWinXwaylandServerModule PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(KWinXwaylandServerModule PUBLIC kwin KWinXwaylandCommon)
