/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QSize>

namespace KWin
{
class ClientBufferInternal;

namespace QPA
{

class Swapchain
{
public:
    explicit Swapchain(const QSize &size, qreal devicePixelRatio);
    ~Swapchain();

    ClientBufferInternal *acquire();

    QSize size() const;
    qreal devicePixelRatio() const;

private:
    ClientBufferInternal *allocate();

    QList<ClientBufferInternal *> m_buffers;
    QSize m_size;
    qreal m_devicePixelRatio;
};

} // namespace QPA
} // namespace KWin
