#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    slidetabs/slidetabs.cpp
    )

# .desktop files
install( FILES
    slidetabs/slidetabs.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    slidetabs/slidetabs_config.cpp
    slidetabs/slidetabs_config.ui
    )

# .desktop files
install( FILES
    slidetabs/slidetabs_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
