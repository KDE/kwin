#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    coverswitch/coverswitch.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources coverswitch/coverswitchconfig.kcfgc)

# .desktop files
install( FILES
    coverswitch/coverswitch.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

install( FILES
    coverswitch/coverswitch-reflection.glsl
    DESTINATION ${DATA_INSTALL_DIR}/kwin )


#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    coverswitch/coverswitch_config.cpp
    coverswitch/coverswitch_config.ui
    )
kde4_add_kcfg_files(kwin4_effect_builtins_config_sources coverswitch/coverswitchconfig.kcfgc)

# .desktop files
install( FILES
    coverswitch/coverswitch_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
