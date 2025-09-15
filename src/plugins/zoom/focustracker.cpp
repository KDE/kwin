/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focustracker.h"

#if KWIN_BUILD_QACCESSIBILITYCLIENT

namespace KWin
{

FocusTracker::FocusTracker()
    : m_accessibilityRegistry(new QAccessibleClient::Registry(this))
{
    connect(m_accessibilityRegistry, &QAccessibleClient::Registry::focusChanged, this, [this](const QAccessibleClient::AccessibleObject &object) {
        Q_EMIT moved(object.focusPoint());
    });
    m_accessibilityRegistry->subscribeEventListeners(QAccessibleClient::Registry::Focus);
}

} // namespace KWin

#include "moc_focustracker.cpp"

#endif
