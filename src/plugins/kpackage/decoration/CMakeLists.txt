add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_decoration\")

set(decoration_SRCS
    decoration.cpp
)

add_library(kwin_packagestructure_decoration MODULE ${decoration_SRCS})

target_link_libraries(kwin_packagestructure_decoration
   KF5::I18n
   KF5::Package
)

kcoreaddons_desktop_to_json(kwin_packagestructure_decoration kwin-packagestructure-decoration.desktop)

install(TARGETS kwin_packagestructure_decoration DESTINATION ${KDE_INSTALL_PLUGINDIR}/kpackage/packagestructure)

