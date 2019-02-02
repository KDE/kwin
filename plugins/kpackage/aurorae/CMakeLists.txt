add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_aurorae\")

set(aurorae_SRCS
    aurorae.cpp
)

add_library(kwin_packagestructure_aurorae MODULE ${aurorae_SRCS})

target_link_libraries(kwin_packagestructure_aurorae
   KF5::I18n
   KF5::Package
)

kcoreaddons_desktop_to_json(kwin_packagestructure_aurorae kwin-packagestructure-aurorae.desktop)

install(TARGETS kwin_packagestructure_aurorae DESTINATION ${KDE_INSTALL_PLUGINDIR}/kpackage/packagestructure)

