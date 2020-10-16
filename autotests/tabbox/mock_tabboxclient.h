/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MOCK_TABBOX_CLIENT_H
#define KWIN_MOCK_TABBOX_CLIENT_H

#include "../../tabbox/tabboxhandler.h"

#include <QIcon>
#include <QUuid>

namespace KWin
{
class MockTabBoxClient : public TabBox::TabBoxClient
{
public:
    explicit MockTabBoxClient(QString caption);
    bool isMinimized() const override {
        return false;
    }
    QString caption() const override {
        return m_caption;
    }
    void close() override;
    int height() const override {
        return 100;
    }
    virtual QPixmap icon(const QSize &size = QSize(32, 32)) const {
        return QPixmap(size);
    }
    bool isCloseable() const override {
        return true;
    }
    bool isFirstInTabBox() const override {
        return false;
    }
    int width() const override {
        return 100;
    }
    int x() const override {
        return 0;
    }
    int y() const override {
        return 0;
    }
    QIcon icon() const override {
        return QIcon();
    }

    QUuid internalId() const override {
        return QUuid{};
    }

private:
    QString m_caption;
};
} // namespace KWin
#endif
