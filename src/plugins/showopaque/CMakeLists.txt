# SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(showopaque_SOURCES
    main.cpp
    showopaque.cpp
)

kwin_add_builtin_effect(showopaque ${showopaque_SOURCES})

target_link_libraries(showopaque PRIVATE
    kwin
)
