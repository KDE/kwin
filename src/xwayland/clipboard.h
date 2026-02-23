/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "selection.h"

namespace KWin::Xwl
{

/**
 * Represents the X clipboard, which is on Wayland side just called
 * @e selection.
 */
class Clipboard : public Selection
{
    Q_OBJECT

public:
    Clipboard(xcb_atom_t atom, QObject *parent);
    ~Clipboard() override;

private:
    void selectionDisowned() override;
    void selectionClaimed(xcb_xfixes_selection_notify_event_t *event) override;
    void targetsReceived(const QStringList &mimeTypes) override;

    void onSelectionChanged();
    void onActiveWindowChanged();

    bool x11ClientsCanAccessSelection() const;
};

} // namespace KWin::Xwl
