#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    thumbnailaside/thumbnailaside.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources thumbnailaside/thumbnailasideconfig.kcfgc)

# .desktop files
install( FILES
    thumbnailaside/thumbnailaside.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    thumbnailaside/thumbnailaside_config.cpp
    thumbnailaside/thumbnailaside_config.ui
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources thumbnailaside/thumbnailasideconfig.kcfgc)

# .desktop files
install( FILES
    thumbnailaside/thumbnailaside_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
