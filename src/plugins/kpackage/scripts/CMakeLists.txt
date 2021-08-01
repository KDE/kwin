add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_scripts\")

kcoreaddons_add_plugin(kwin_packagestructure_scripts SOURCES scripts.cpp INSTALL_NAMESPACE "kpackage/packagestructure")

target_link_libraries(kwin_packagestructure_scripts
   KF5::I18n
   KF5::Package
)
set_target_properties(kwin_packagestructure_scripts PROPERTIES OUTPUT_NAME kwin_script)
