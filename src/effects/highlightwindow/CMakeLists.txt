#######################################
# Effect

# Source files
set(highlightwindow_SOURCES
    highlightwindow.cpp
    main.cpp
)

kwin_add_builtin_effect(highlightwindow ${highlightwindow_SOURCES})
target_link_libraries(highlightwindow PRIVATE
    kwineffects

    Qt::DBus
)
