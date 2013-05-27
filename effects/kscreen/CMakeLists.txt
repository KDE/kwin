#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
     kscreen/kscreen.cpp
)

kde4_add_kcfg_files(kwin4_effect_builtins_sources kscreen/kscreenconfig.kcfgc)

# .desktop files
install( FILES
         kscreen/kscreen.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
