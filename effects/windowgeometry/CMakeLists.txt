#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    windowgeometry/windowgeometry.cpp
    )

# .desktop files
install( FILES
    windowgeometry/windowgeometry.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    windowgeometry/windowgeometry_config.cpp
    windowgeometry/windowgeometry_config.ui
    )

# .desktop files
install( FILES
    windowgeometry/windowgeometry_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
