/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"

#if KWIN_BUILD_QACCESSIBILITYCLIENT

#include <qaccessibilityclient/registry.h>

namespace KWin
{

/**
 * The FocusTracker helper provides a way to track the center of currently focused object, e.g.
 * center of a button or a text field, etc. In other words, the FocusTracker will fire when you
 * press the Tab key, and so on.
 */
class FocusTracker : public QObject
{
    Q_OBJECT

public:
    FocusTracker();

Q_SIGNALS:
    void moved(const QPointF &point);

private:
    QAccessibleClient::Registry *m_accessibilityRegistry = nullptr;
};

} // namespace KWin

#endif
