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
} // namespace KWin

#include "moc_abstract_data_source.cpp"
