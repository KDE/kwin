#######################################
# Effect

# Source files
set(fallapart_SOURCES
    fallapart.cpp
    main.cpp
)

kconfig_add_kcfg_files(fallapart_SOURCES
    fallapartconfig.kcfgc
)

kwin_add_builtin_effect(fallapart ${fallapart_SOURCES})
target_link_libraries(fallapart PRIVATE
    kwin

    KF6::ConfigGui
)
