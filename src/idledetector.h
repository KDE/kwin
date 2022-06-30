/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <QTimer>

namespace KWin
{

class KWIN_EXPORT IdleDetector : public QObject
{
    Q_OBJECT

public:
    explicit IdleDetector(std::chrono::milliseconds timeout, QObject *parent = nullptr);
    ~IdleDetector() override;

    void activity();

    bool isInhibited() const;
    void setInhibited(bool inhibited);

Q_SIGNALS:
    void idle();
    void resumed();

private:
    void markAsIdle();
    void markAsResumed();

    QTimer *m_timer;
    bool m_isIdle = false;
    bool m_isInhibited = false;
};

} // namespace KWin
