/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"

#include <QObject>
#include <QSize>

namespace KWin
{

class KWIN_EXPORT ClientBuffer : public QObject
{
    Q_OBJECT

public:
    explicit ClientBuffer(QObject *parent = nullptr);

    bool isDestroyed() const;
    bool isReferenced() const;

    void ref();
    void unref();

    void markAsDestroyed();

    virtual bool hasAlphaChannel() const = 0;
    virtual QSize size() const = 0;

protected:
    virtual void release() = 0;

private:
    int m_refCount = 0;
    bool m_isDestroyed = false;
};

} // namespace KWin
