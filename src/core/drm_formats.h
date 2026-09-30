/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QHash>
#include <QList>
#include <QString>
#include <unordered_map>
#include <vector>

#include <epoxy/gl.h>
#include <libdrm/drm_fourcc.h>
#include <optional>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace KWin
{

// TODO once FreeBSD is on clang 21+, switch this to use std::flat_set
class KWIN_EXPORT ModifierList : public QList<uint64_t>
{
public:
    ModifierList();
    ModifierList(QList<uint64_t> &&move);
    ModifierList(const QList<uint64_t> &copy);
    ModifierList(const std::initializer_list<uint64_t> &list);

    void insert(uint64_t modifier);
    void erase(uint64_t modifier);
    void insert(const ModifierList &other);

    void intersect(const ModifierList &other);
    ModifierList intersected(const ModifierList &other) const;
};

class KWIN_EXPORT FormatModifierMap : public QHash<uint32_t, ModifierList>
{
public:
    FormatModifierMap();
    FormatModifierMap(QHash<uint32_t, ModifierList> &&move);
    FormatModifierMap(const QHash<uint32_t, ModifierList> &copy);
    FormatModifierMap(const std::initializer_list<std::pair<uint32_t, ModifierList>> &list);

    void merge(const FormatModifierMap &other);
    FormatModifierMap merged(const FormatModifierMap &other) const;
    FormatModifierMap intersected(const FormatModifierMap &other) const;

    bool containsFormat(uint32_t format, uint64_t modifier) const;
};

struct KWIN_EXPORT PlaneLayout
{
    uint32_t bitsPerPixel;
    uint32_t sizeDivisor;
};

struct KWIN_EXPORT DmabufLayout
{
    uint32_t count;
    std::array<PlaneLayout, 4> planes;
};

struct KWIN_EXPORT FormatInfo
{
    uint32_t drmFormat;
    uint32_t bitsPerColor;
    uint32_t alphaBits;
    uint32_t bitsPerPixel;
    GLint openglFormat;
    VkFormat vulkanFormat;
    VkComponentMapping swizzles;
    bool floatingPoint;
    bool yuv;
    std::optional<DmabufLayout> planeLayout;

    static const std::unordered_map<uint32_t, FormatInfo> s_knownFormats;

    static std::optional<FormatInfo> get(uint32_t drmFormat);
    static QString drmFormatName(uint32_t format);
};

}

QDebug &operator<<(QDebug &s, const KWin::FormatModifierMap &map);
