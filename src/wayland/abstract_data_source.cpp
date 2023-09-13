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

} // namespace KWin

#include "moc_abstract_data_source.cpp"
