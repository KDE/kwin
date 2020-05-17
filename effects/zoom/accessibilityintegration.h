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
