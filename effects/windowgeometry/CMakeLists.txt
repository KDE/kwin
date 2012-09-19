#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    windowgeometry/windowgeometry.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources windowgeometry/windowgeometryconfig.kcfgc)

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

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources windowgeometry/windowgeometryconfig.kcfgc)

# .desktop files
install( FILES
    windowgeometry/windowgeometry_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
