/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <qaccessibilityclient/registry.h>

namespace KWin
{

class ZoomAccessibilityIntegration : public QObject
{
    Q_OBJECT

public:
    explicit ZoomAccessibilityIntegration(QObject *parent = nullptr);

    void setFocusTrackingEnabled(bool enabled);
    bool isFocusTrackingEnabled() const;

    void setTextCaretTrackingEnabled(bool enabled);
    bool isTextCaretTrackingEnabled() const;

Q_SIGNALS:
    void focusPointChanged(const QPoint &point);

private Q_SLOTS:
    void slotFocusChanged(const QAccessibleClient::AccessibleObject &object);

private:
    void createAccessibilityRegistry();
    void destroyAccessibilityRegistry();
    void updateAccessibilityRegistry();

    QAccessibleClient::Registry *m_accessibilityRegistry = nullptr;
    bool m_isFocusTrackingEnabled = false;
    bool m_isTextCaretTrackingEnabled = false;
};

} // namespace KWin
