#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    flipswitch/flipswitch.cpp
    )

# .desktop files
install( FILES
    flipswitch/flipswitch.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    flipswitch/flipswitch_config.cpp
    flipswitch/flipswitch_config.ui
    )

# .desktop files
install( FILES
    flipswitch/flipswitch_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
