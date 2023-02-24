add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_aurorae\")

kcoreaddons_add_plugin(kwin_packagestructure_aurorae SOURCES aurorae.cpp INSTALL_NAMESPACE "kpackage/packagestructure")

target_link_libraries(kwin_packagestructure_aurorae
   KF6::I18n
   KF6::Package
)
set_target_properties(kwin_packagestructure_aurorae PROPERTIES OUTPUT_NAME kwin_aurorae)
