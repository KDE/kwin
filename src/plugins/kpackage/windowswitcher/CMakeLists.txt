add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_windowswitcher\")

kcoreaddons_add_plugin(kwin_packagestructure_windowswitcher SOURCES windowswitcher.cpp INSTALL_NAMESPACE "kpackage/packagestructure")

target_link_libraries(kwin_packagestructure_windowswitcher
   KF6::I18n
   KF6::Package
)
set_target_properties(kwin_packagestructure_windowswitcher PROPERTIES OUTPUT_NAME kwin_windowswitcher)
