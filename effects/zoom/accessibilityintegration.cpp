/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
