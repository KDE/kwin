#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    logout/logout.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources logout/logoutconfig.kcfgc)

# .desktop files
install( FILES
    logout/logout.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    logout/data/1.10/vignetting.frag
    logout/data/1.10/logout-blur.frag
    DESTINATION ${DATA_INSTALL_DIR}/kwin/shaders/1.10
)

install( FILES
    logout/data/1.40/vignetting.frag
    logout/data/1.40/logout-blur.frag
    DESTINATION ${DATA_INSTALL_DIR}/kwin/shaders/1.40
)
