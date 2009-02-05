#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    login/login.cpp
    )

# .desktop files
install( FILES
    login/login.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
