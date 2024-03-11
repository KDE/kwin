kcoreaddons_add_plugin(duhast INSTALL_NAMESPACE "kwin/plugins")
target_sources(duhast PRIVATE
    bashdetector.cpp
    main.cpp
    duhast.cpp
)
kconfig_add_kcfg_files(duhast
    duhastconfig.kcfgc
)

target_link_libraries(duhast kwin Qt6::Multimedia KF6::ConfigGui)
