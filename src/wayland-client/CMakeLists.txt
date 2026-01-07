target_sources(kwin PRIVATE
    linuxdmabuf.cpp
    viewporter.cpp
)
ecm_qt_declare_logging_category(kwin
    HEADER waylandclientlogging.h
    IDENTIFIER KWIN_WAYLAND_CLIENT
    CATEGORY_NAME kwin_wayland_client
)
