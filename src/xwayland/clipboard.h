/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "selection.h"

#include <memory>

namespace KWin
{

class AbstractDataSource;

namespace Xwl
{
class XwlDataSource;

/**
 * Represents the X clipboard, which is on Wayland side just called
 * @e selection.
 */
class Clipboard : public Selection
{
    Q_OBJECT

public:
    Clipboard(xcb_atom_t atom, QObject *parent);

private:
    void doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event) override;
    void x11OfferLost() override;
    void x11TargetsReceived(const QStringList &mimeTypes) override;

    void onSelectionChanged();
    void onActiveWindowChanged();

    /**
     * Returns if dsi is managed by our data bridge
     */
    bool ownsSelection(AbstractDataSource *dsi) const;

    bool x11ClientsCanAccessSelection() const;

    Q_DISABLE_COPY(Clipboard)
    std::unique_ptr<XwlDataSource> m_selectionSource;
};

} // namespace Xwl
} // namespace KWin
