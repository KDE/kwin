# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

kwin_add_builtin_effect(colorblindnesscorrection)

target_sources(colorblindnesscorrection PRIVATE
    main.cpp
    colorblindnesscorrection.cpp
    colorblindnesscorrection.qrc
)

kconfig_add_kcfg_files(colorblindnesscorrection
    colorblindnesscorrectionconfig.kcfgc
)

target_link_libraries(colorblindnesscorrection PRIVATE
    kwin
    KF6::ConfigGui
)
