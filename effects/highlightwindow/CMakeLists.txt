#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    highlightwindow/highlightwindow.cpp
    )

# .desktop files
install( FILES
    highlightwindow/highlightwindow.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
