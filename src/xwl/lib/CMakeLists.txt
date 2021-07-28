ecm_qt_declare_logging_category(xwaylandliblogging_SOURCES
    HEADER
        xwayland_logging.h
    IDENTIFIER
        KWIN_XWL
    CATEGORY_NAME
        kwin_xwl
    DEFAULT_SEVERITY
        Warning
)

add_library(KWinXwaylandCommon STATIC
    xwaylandsocket.cpp
    xauthority.cpp
    ${xwaylandliblogging_SOURCES}
)

target_include_directories(KWinXwaylandCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(KWinXwaylandCommon Qt::Core Qt::Network)
