/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "abstract_data_source.h"

namespace KWin
{

AbstractDataSource::AbstractDataSource(QObject *parent)
    : QObject(parent)
{
}

void AbstractDataSource::setKeyboardModifiers(Qt::KeyboardModifiers heldModifiers)
{
    if (m_heldModifiers == heldModifiers) {
        return;
    }
    m_heldModifiers = heldModifiers;
    Q_EMIT keyboardModifiersChanged();
}

Qt::KeyboardModifiers AbstractDataSource::keyboardModifiers() const
{
    return m_heldModifiers;
}

void AbstractDataSource::setExclusiveAction(DnDAction action)
{
    if (m_exclusiveAction != action) {
        m_exclusiveAction = action;
        Q_EMIT exclusiveActionChanged();
    }
}

std::optional<DnDAction> AbstractDataSource::exclusiveAction() const
{
    return m_exclusiveAction;
}

} // namespace KWin

#include "moc_abstract_data_source.cpp"
