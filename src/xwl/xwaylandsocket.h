/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QString>

namespace KWin
{

class XwaylandSocket
{
public:
    XwaylandSocket();
    ~XwaylandSocket();

    bool isValid() const;
    int display() const;
    QString name() const;

    int unixFileDescriptor() const;
    int abstractFileDescriptor() const;

private:
    int m_unixFileDescriptor = -1;
    int m_abstractFileDescriptor = -1;
    int m_display = -1;
    QString m_socketFilePath;
    QString m_lockFilePath;
};

} // namespace KWin
