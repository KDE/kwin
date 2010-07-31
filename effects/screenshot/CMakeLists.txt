#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    screenshot/screenshot.cpp
    )

# .desktop files
install( FILES
    screenshot/screenshot.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
