add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_effect\")

set(effect_SRCS
    effect.cpp
)

add_library(kwin_packagestructure_effect MODULE ${effect_SRCS})

target_link_libraries(kwin_packagestructure_effect
   KF5::I18n
   KF5::Package
)

kcoreaddons_desktop_to_json(kwin_packagestructure_effect kwin-packagestructure-effect.desktop)

install(TARGETS kwin_packagestructure_effect DESTINATION ${KDE_INSTALL_PLUGINDIR}/kpackage/packagestructure)
