#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    swiveltabs/swiveltabs.cpp
    )

# .desktop files
install( FILES
    swiveltabs/swiveltabs.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    swiveltabs/swiveltabs_config.cpp
    swiveltabs/swiveltabs_config.ui
    )

# .desktop files
install( FILES
    swiveltabs/swiveltabs_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
