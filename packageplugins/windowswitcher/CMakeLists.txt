add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_windowswitcher\")

set(windowswitcher_SRCS
    windowswitcher.cpp
)

add_library(kwin_packagestructure_windowswitcher MODULE ${windowswitcher_SRCS})

target_link_libraries(kwin_packagestructure_windowswitcher
   KF5::I18n
   KF5::Package
)

kcoreaddons_desktop_to_json(kwin_packagestructure_windowswitcher kwin-packagestructure-windowswitcher.desktop)

install(TARGETS kwin_packagestructure_windowswitcher DESTINATION ${KDE_INSTALL_PLUGINDIR}/kpackage/packagestructure)
