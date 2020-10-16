/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_TABBOX_CLIENT_MODEL_H
#define TEST_TABBOX_CLIENT_MODEL_H
#include <QObject>

class TestTabBoxClientModel : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    /**
     * Tests that calculating the longest caption does not
     * crash in case the internal m_clientList contains a weak
     * pointer to a deleted TabBoxClient.
     *
     * See bug #303840
     */
    void testLongestCaptionWithNullClient();
    /**
     * Tests the creation of the Client list for the case that
     * there is no active Client, but that Clients actually exist.
     *
     * See BUG: 305449
     */
    void testCreateClientListNoActiveClient();
    /**
     * Tests the creation of the Client list for the case that
     * the active Client is not in the Focus chain.
     *
     * See BUG: 306260
     */
    void testCreateClientListActiveClientNotInFocusChain();
};

#endif
