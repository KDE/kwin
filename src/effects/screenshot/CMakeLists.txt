#######################################
# Effect

# Source files
set(screenshot_SOURCES
    main.cpp
    screenshot.cpp
    screenshotdbusinterface2.cpp
)

qt_add_dbus_adaptor(screenshot_SOURCES org.kde.KWin.ScreenShot2.xml screenshotdbusinterface2.h KWin::ScreenShotDBusInterface2)

kwin4_add_effect_module(screenshot ${screenshot_SOURCES})
target_link_libraries(screenshot PRIVATE
    kwineffects
    kwinglutils

    KF6::Service
    KF6::I18n

    Qt::Concurrent
    Qt::DBus
)

ecm_qt_declare_logging_category(screenshot
    HEADER screenshotlogging.h
    IDENTIFIER KWIN_SCREENSHOT
    CATEGORY_NAME kwin_effect_screenshot
    DEFAULT_SEVERITY Warning
)
