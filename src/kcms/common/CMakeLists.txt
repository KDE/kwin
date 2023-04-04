# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwincommon\")

set(kcmkwincommon_SRC
    effectsmodel.cpp
)

qt_add_dbus_interface(kcmkwincommon_SRC
    ${KWin_SOURCE_DIR}/src/org.kde.kwin.Effects.xml kwin_effects_interface
)

add_library(kcmkwincommon SHARED ${kcmkwincommon_SRC})

target_link_libraries(kcmkwincommon
    Qt::Core
    Qt::DBus
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
    KF6::Package
    KF6::KCMUtils
)

set_target_properties(kcmkwincommon PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS kcmkwincommon ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)

set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
qt_add_dbus_interface(kcm_kwin4_genericscripted_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
add_library(kcm_kwin4_genericscripted MODULE ${kcm_kwin4_genericscripted_SRCS})
target_link_libraries(kcm_kwin4_genericscripted
    KF6::KCMUtils #KCModule
    KF6::I18n
    Qt::DBus
    Qt::UiTools
)
install(TARGETS kcm_kwin4_genericscripted DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs)
