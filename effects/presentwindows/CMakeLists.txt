#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    presentwindows/presentwindows.cpp
    presentwindows/presentwindows_proxy.cpp
    )

# .desktop files
install( FILES
    presentwindows/presentwindows.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    presentwindows/presentwindows_config.cpp
    presentwindows/presentwindows_config.ui
    )

# .desktop files
install( FILES
    presentwindows/presentwindows_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
