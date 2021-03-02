#######################################
# Effect

# Source files
set(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    ../service_utils.cpp
    screenshot/screenshot.cpp
    screenshot/screenshotdbusinterface1.cpp
    screenshot/screenshotdbusinterface2.cpp
)

qt5_add_dbus_adaptor(kwin4_effect_builtins_sources screenshot/org.kde.KWin.ScreenShot2.xml screenshot/screenshotdbusinterface2.h KWin::ScreenShotDBusInterface2)
