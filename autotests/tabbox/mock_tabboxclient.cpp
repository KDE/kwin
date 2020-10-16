/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_tabboxclient.h"
#include "mock_tabboxhandler.h"

namespace KWin
{

MockTabBoxClient::MockTabBoxClient(QString caption)
    : TabBoxClient()
    , m_caption(caption)
{
}

void MockTabBoxClient::close()
{
    static_cast<MockTabBoxHandler*>(TabBox::tabBox)->closeWindow(this);
}

} // namespace KWin
