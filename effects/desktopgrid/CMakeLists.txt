#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    desktopgrid/desktopgrid.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources desktopgrid/desktopgridconfig.kcfgc)

# .desktop files
install( FILES
    desktopgrid/desktopgrid.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    desktopgrid/desktopgrid_config.cpp
    desktopgrid/desktopgrid_config.ui
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources desktopgrid/desktopgridconfig.kcfgc)

# .desktop files
install( FILES
    desktopgrid/desktopgrid_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
