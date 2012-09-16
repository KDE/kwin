#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    zoom/zoom.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources zoom/zoomconfig.kcfgc)

# .desktop files
install( FILES
    zoom/zoom.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    zoom/zoom_config.cpp
    zoom/zoom_config.ui
    )
kde4_add_kcfg_files(kwin4_effect_builtins_config_sources zoom/zoomconfig.kcfgc)

# .desktop files
install( FILES
    zoom/zoom_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
