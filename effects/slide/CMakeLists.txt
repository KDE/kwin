#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    slide/slide.cpp
    )

# .desktop files
install( FILES
    slide/slide.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
