#########################################################################
# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwincompositing\")

################# configure checks and create the configured files #################

set(kwincompositing_SRC
    main.cpp
    kwincompositingdata.cpp
)

kconfig_add_kcfg_files(kwincompositing_SRC kwincompositing_setting.kcfgc GENERATE_MOC)

qt_add_dbus_interface(kwincompositing_SRC
    ${KWin_SOURCE_DIR}/src/org.kde.kwin.Compositing.xml kwin_compositing_interface
)

ki18n_wrap_ui(kwincompositing_SRC compositing.ui)

add_library(kwincompositing MODULE ${kwincompositing_SRC})

target_link_libraries(kwincompositing
    Qt::DBus
    Qt::Widgets

    KF5::ConfigCore
    KF5::CoreAddons
    KF5::I18n
    KF5::KCMUtils
    KF5::WindowSystem
)

install(TARGETS kwincompositing DESTINATION ${KDE_INSTALL_PLUGINDIR})
install(FILES kwincompositing.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
