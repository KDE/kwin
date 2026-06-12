/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QObject>
#include <QPointer>
#include <QSizeF>
#include <optional>

namespace KWin
{

class OverlayShellSurfaceV1;
class Window;
class SurfaceItem;

class KWIN_EXPORT WindowOverlay : public QObject
{
    Q_OBJECT

public:
    explicit WindowOverlay(OverlayShellSurfaceV1 *shell, Window *parent);
    ~WindowOverlay() override;

private:
    void configure();
    void ackConfigure(uint32_t serial);
    void handleCommit();
    void destroy();

    OverlayShellSurfaceV1 *const m_shell;
    QPointer<Window> m_parent;
    std::optional<QSizeF> m_lastConfigureSize;
    std::optional<uint32_t> m_lastAckedConfigure;
    uint32_t m_serial = 0;
    std::unique_ptr<SurfaceItem> m_item;
};

}
