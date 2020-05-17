/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "accessibilityintegration.h"

using namespace QAccessibleClient; // Whatever, sue me...

namespace KWin
{

ZoomAccessibilityIntegration::ZoomAccessibilityIntegration(QObject *parent)
    : QObject(parent)
{
}

void ZoomAccessibilityIntegration::setFocusTrackingEnabled(bool enabled)
{
    if (m_isFocusTrackingEnabled == enabled) {
        return;
    }
    m_isFocusTrackingEnabled = enabled;
    updateAccessibilityRegistry();
}

bool ZoomAccessibilityIntegration::isFocusTrackingEnabled() const
{
    return m_isFocusTrackingEnabled;
}

void ZoomAccessibilityIntegration::setTextCaretTrackingEnabled(bool enabled)
{
    if (m_isTextCaretTrackingEnabled == enabled) {
        return;
    }
    m_isTextCaretTrackingEnabled = enabled;
    updateAccessibilityRegistry();
}

bool ZoomAccessibilityIntegration::isTextCaretTrackingEnabled() const
{
    return m_isTextCaretTrackingEnabled;
}

void ZoomAccessibilityIntegration::updateAccessibilityRegistry()
{
    Registry::EventListeners eventListeners = Registry::NoEventListeners;

    if (isTextCaretTrackingEnabled()) {
        eventListeners |= Registry::TextCaretMoved;
    }
    if (isFocusTrackingEnabled()) {
        eventListeners |= Registry::Focus;
    }

    if (eventListeners == Registry::NoEventListeners) {
        destroyAccessibilityRegistry();
        return;
    }
    if (!m_accessibilityRegistry) {
        createAccessibilityRegistry();
    }

    m_accessibilityRegistry->subscribeEventListeners(eventListeners);
}

void ZoomAccessibilityIntegration::createAccessibilityRegistry()
{
    m_accessibilityRegistry = new Registry(this);

    connect(m_accessibilityRegistry, &Registry::textCaretMoved,
            this, &ZoomAccessibilityIntegration::slotFocusChanged);
    connect(m_accessibilityRegistry, &Registry::focusChanged,
            this, &ZoomAccessibilityIntegration::slotFocusChanged);
}

void ZoomAccessibilityIntegration::destroyAccessibilityRegistry()
{
    if (!m_accessibilityRegistry) {
        return;
    }

    disconnect(m_accessibilityRegistry, nullptr, this, nullptr);

    m_accessibilityRegistry->deleteLater();
    m_accessibilityRegistry = nullptr;
}

void ZoomAccessibilityIntegration::slotFocusChanged(const AccessibleObject &object)
{
    emit focusPointChanged(object.focusPoint());
}

} // namespace KWin
