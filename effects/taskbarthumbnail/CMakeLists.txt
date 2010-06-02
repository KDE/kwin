#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    taskbarthumbnail/taskbarthumbnail.cpp
    )

# .desktop files
install( FILES
    taskbarthumbnail/taskbarthumbnail.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
