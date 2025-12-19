# SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

kwin_add_builtin_effect(slidingnotifications)

target_sources(slidingnotifications PRIVATE
    main.cpp
    slidingnotifications.cpp
)

target_link_libraries(slidingnotifications PRIVATE
    kwin
)
