/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

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
class Primary : public Selection
{
    Q_OBJECT

public:
    Primary(xcb_atom_t atom, QObject *parent);
    ~Primary() override;

private:
    void doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event) override;
    void x11OfferLost() override;
    void x11OffersChanged(const QStringList &added, const QStringList &removed) override;
    /**
     * Check the current state of the selection and if a source needs
     * to be created or destroyed.
     */
    void checkWlSource();

    /**
     * Returns if dsi is managed by our data bridge
     */
    bool ownsSelection(AbstractDataSource *dsi) const;

    Q_DISABLE_COPY(Primary)
    std::unique_ptr<XwlDataSource> m_primarySelectionSource;
};

} // namespace Xwl
} // namespace KWin
