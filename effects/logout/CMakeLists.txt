#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    logout/logout.cpp
    )

# .desktop files
install( FILES
    logout/logout.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    logout/data/vignetting.frag
    logout/data/logout-blur.frag
    DESTINATION ${DATA_INSTALL_DIR}/kwin )
