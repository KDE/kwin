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
