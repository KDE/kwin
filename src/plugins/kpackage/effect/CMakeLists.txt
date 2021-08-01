add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_effect\")

kcoreaddons_add_plugin(kwin_packagestructure_effect SOURCES effect.cpp INSTALL_NAMESPACE "kpackage/packagestructure")

target_link_libraries(kwin_packagestructure_effect
   KF5::I18n
   KF5::Package
)
set_target_properties(kwin_packagestructure_effect PROPERTIES OUTPUT_NAME kwin_effect)
