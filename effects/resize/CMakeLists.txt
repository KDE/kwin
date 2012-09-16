#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    resize/resize.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources resize/resizeconfig.kcfgc)

# .desktop files
install( FILES
    resize/resize.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    resize/resize_config.cpp
    resize/resize_config.ui
    )
kde4_add_kcfg_files(kwin4_effect_builtins_config_sources resize/resizeconfig.kcfgc)

# .desktop files
install( FILES
    resize/resize_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
