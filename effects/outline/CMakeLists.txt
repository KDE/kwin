#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    outline/outline.cpp
    )

# .desktop files
install( FILES
    outline/outline.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
