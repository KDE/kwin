#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    fadedesktop/fadedesktop.cpp
    )

# .desktop files
install( FILES
    fadedesktop/fadedesktop.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
