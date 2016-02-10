#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    logout/logout.cpp
    )

kconfig_add_kcfg_files(kwin4_effect_builtins_sources logout/logoutconfig.kcfgc)
