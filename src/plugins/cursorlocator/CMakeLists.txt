# SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(CursorLocatorPlugin INSTALL_NAMESPACE "kwin/plugins")

target_sources(CursorLocatorPlugin PRIVATE
    main.cpp
    cursorlocator.cpp
)
target_link_libraries(CursorLocatorPlugin PRIVATE kwin)

