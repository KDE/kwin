# SPDX-FileCopyrightText: 2025 Jakob Petsovits <jpetso@petsovits.com>
#
# SPDX-License-Identifier: BSD-3-Clause

kconfig_add_kcfg_files(strokeeffectconfig_SOURCES
    strokeeffectconfig.kcfgc
)

kwin_add_builtin_effect(stroke
    main.cpp
    strokeeffect.cpp
    ${strokeeffectconfig_SOURCES}
)
qt6_add_qml_module(stroke
    NO_PLUGIN
    URI stroke
    RESOURCE_PREFIX /
    QML_FILES
        qml/main.qml
        qml/StrokeShape.qml
)
target_link_libraries(stroke PRIVATE
    kwin
    KF6::ConfigGui
    Qt::Quick
)
