#######################################
# Effect

# Source files
set(highlightwindow_SOURCES
    highlightwindow.cpp
    main.cpp
)

kwin4_add_effect_module(highlightwindow ${highlightwindow_SOURCES})
target_link_libraries(highlightwindow PRIVATE
    kwineffects

    Qt::DBus
)
