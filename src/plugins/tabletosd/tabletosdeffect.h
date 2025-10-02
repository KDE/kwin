/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/quickeffect.h"

#include <QKeySequence>

class QAction;

namespace KWin
{

struct TabletButton
{
    Q_GADGET
    QML_ELEMENT
    QML_VALUE_TYPE(tabletButton)

public:
    // position
    QString boundButtonName;

    explicit TabletButton(const QString &name)
        : boundButtonName(name)
    {
    }
};

class TabletOsdEffect : public QuickSceneEffect
{
    Q_OBJECT

    Q_PROPERTY(QList<TabletButton> topButtons MEMBER m_topButtons CONSTANT)
    Q_PROPERTY(QList<TabletButton> bottomButtons MEMBER m_bottomButtons CONSTANT)
    Q_PROPERTY(QList<TabletButton> leftButtons MEMBER m_leftButtons CONSTANT)
    Q_PROPERTY(QList<TabletButton> rightButtons MEMBER m_rightButtons CONSTANT)

public:
    TabletOsdEffect();
    ~TabletOsdEffect() override;

private:
    QList<TabletButton> m_topButtons;
    QList<TabletButton> m_bottomButtons;
    QList<TabletButton> m_leftButtons;
    QList<TabletButton> m_rightButtons;
};

} // namespace KWin
