#######################################
# Effect

# Source files
set(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    kscreen/kscreen.cpp
)

kconfig_add_kcfg_files(kwin4_effect_builtins_sources kscreen/kscreenconfig.kcfgc)

