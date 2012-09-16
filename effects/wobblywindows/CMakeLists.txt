#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    wobblywindows/wobblywindows.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources wobblywindows/wobblywindowsconfig.kcfgc)

# .desktop files
install( FILES
    wobblywindows/wobblywindows.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    wobblywindows/wobblywindows_config.cpp
    wobblywindows/wobblywindows_config.ui
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources wobblywindows/wobblywindowsconfig.kcfgc)

# .desktop files
install( FILES
    wobblywindows/wobblywindows_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
