add_library(KWinSplitscreenPlugin SHARED)
target_sources(KWinSplitscreenPlugin PRIVATE
    main.cpp
    splitscreen.cpp
)

ecm_qt_declare_logging_category(KWinSplitscreenPlugin
    HEADER kwinsplitscreen_logging.h
    IDENTIFIER KWIN_SPLITSCREEN
    CATEGORY_NAME kwin_splitscreen
    DEFAULT_SEVERITY Warning
)

target_link_libraries(KWinSplitscreenPlugin kwin)
install(TARGETS KWinSplitscreenPlugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/plugins/)
