#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    boxswitch/boxswitch.cpp
    boxswitch/boxswitch_proxy.cpp
    )

# .desktop files
install( FILES
    boxswitch/boxswitch.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    boxswitch/boxswitch_config.cpp
    boxswitch/boxswitch_config.ui
    )

# .desktop files
install( FILES
    boxswitch/boxswitch_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
