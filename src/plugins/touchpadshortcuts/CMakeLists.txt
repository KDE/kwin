# SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(TouchpadShortcutsPlugin INSTALL_NAMESPACE "${KWIN_PLUGINDIR}/plugins")

target_compile_definitions(TouchpadShortcutsPlugin PRIVATE
    -DTRANSLATION_DOMAIN=\"kwin\"
)

target_sources(TouchpadShortcutsPlugin PRIVATE
    main.cpp
    touchpadshortcuts.cpp
)
target_link_libraries(TouchpadShortcutsPlugin PRIVATE kwin KF6::GlobalAccel KF6::I18n)

