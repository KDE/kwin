#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    resize/resize.cpp
    )

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

# .desktop files
install( FILES
    resize/resize_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
