add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_decoration\")

kcoreaddons_add_plugin(kwin_packagestructure_decoration SOURCES decoration.cpp INSTALL_NAMESPACE "kpackage/packagestructure")

target_link_libraries(kwin_packagestructure_decoration
   KF5::I18n
   KF5::Package
)
set_target_properties(kwin_packagestructure_decoration PROPERTIES OUTPUT_NAME kwin_decoration)
