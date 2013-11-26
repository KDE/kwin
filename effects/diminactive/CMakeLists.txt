#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    diminactive/diminactive.cpp
    )

kconfig_add_kcfg_files(kwin4_effect_builtins_sources diminactive/diminactiveconfig.kcfgc)

# .desktop files
install( FILES
    diminactive/diminactive.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    diminactive/diminactive_config.cpp
    diminactive/diminactive_config.ui
    )

kconfig_add_kcfg_files(kwin4_effect_builtins_config_sources diminactive/diminactiveconfig.kcfgc)

# .desktop files
install( FILES
    diminactive/diminactive_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
