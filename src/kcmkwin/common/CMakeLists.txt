# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwincommon\")

include_directories(${KWin_SOURCE_DIR}/src/effects)

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
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::I18n
    KF5::Package
    KF5::KCMUtils
)

set_target_properties(kcmkwincommon PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS kcmkwincommon ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
