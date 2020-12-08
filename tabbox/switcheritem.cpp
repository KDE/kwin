/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "switcheritem.h"
// KWin
#include "tabboxhandler.h"
#include "screens.h"
// Qt
#include <QAbstractItemModel>

namespace KWin
{
namespace TabBox
{

SwitcherItem::SwitcherItem(QObject *parent)
    : QObject(parent)
    , m_model(nullptr)
    , m_item(nullptr)
    , m_visible(false)
    , m_allDesktops(false)
    , m_currentIndex(0)
{
    m_selectedIndexConnection = connect(tabBox, &TabBoxHandler::selectedIndexChanged, [this] {
        if (isVisible()) {
            setCurrentIndex(tabBox->currentIndex().row());
        }
    });
    connect(screens(), &Screens::changed, this, &SwitcherItem::screenGeometryChanged);
}

SwitcherItem::~SwitcherItem()
{
    disconnect(m_selectedIndexConnection);
}

void SwitcherItem::setItem(QObject *item)
{
    if (m_item == item) {
        return;
    }
    m_item = item;
    emit itemChanged();
}

void SwitcherItem::setModel(QAbstractItemModel *model)
{
    m_model = model;
    emit modelChanged();
}

void SwitcherItem::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    if (visible)
        emit screenGeometryChanged();
    m_visible = visible;
    emit visibleChanged();
}

QRect SwitcherItem::screenGeometry() const
{
    return screens()->geometry(screens()->current());
}

void SwitcherItem::setCurrentIndex(int index)
{
    if (m_currentIndex == index) {
        return;
    }
    m_currentIndex = index;
    if (m_model) {
        tabBox->setCurrentIndex(m_model->index(index, 0));
    }
    emit currentIndexChanged(m_currentIndex);
}

void SwitcherItem::setAllDesktops(bool all)
{
    if (m_allDesktops == all) {
        return;
    }
    m_allDesktops = all;
    emit allDesktopsChanged();
}

void SwitcherItem::setNoModifierGrab(bool set)
{
    if (m_noModifierGrab == set) {
        return;
    }
    m_noModifierGrab = set;
    emit noModifierGrabChanged();
}

}
}
