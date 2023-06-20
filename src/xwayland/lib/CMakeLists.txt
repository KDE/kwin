add_library(KWinXwaylandCommon STATIC
    xwaylandsocket.cpp
    xauthority.cpp
)

ecm_qt_declare_logging_category(KWinXwaylandCommon
    HEADER
        xwayland_logging.h
    IDENTIFIER
        KWIN_XWL
    CATEGORY_NAME
        kwin_xwl
    DEFAULT_SEVERITY
        Warning
)

target_include_directories(KWinXwaylandCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(KWinXwaylandCommon Qt::Core)
