#######################################
# Effect

# Source files
set(highlightwindow_SOURCES
    highlightwindow.cpp highlightwindow.h
    main.cpp main.h
)

kwin4_add_effect_module(kwin4_effect_highlightwindow ${highlightwindow_SOURCES})
target_link_libraries(kwin4_effect_highlightwindow PRIVATE
    kwineffects

    Qt::DBus
)
