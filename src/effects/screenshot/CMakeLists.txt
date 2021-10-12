#######################################
# Effect

# Source files
set(screenshot_SOURCES
    main.cpp
    screenshot.cpp
    screenshotdbusinterface1.cpp
    screenshotdbusinterface2.cpp
)

qt_add_dbus_adaptor(screenshot_SOURCES org.kde.KWin.ScreenShot2.xml screenshotdbusinterface2.h KWin::ScreenShotDBusInterface2)

kwin4_add_effect_module(kwin4_effect_screenshot ${screenshot_SOURCES})
