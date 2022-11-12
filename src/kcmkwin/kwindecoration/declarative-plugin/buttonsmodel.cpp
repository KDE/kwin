/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "buttonsmodel.h"

#include <KLocalizedString>

#include <QFontDatabase>

namespace KDecoration2
{

namespace Preview
{

ButtonsModel::ButtonsModel(const QVector<DecorationButtonType> &buttons, QObject *parent)
    : QAbstractListModel(parent)
    , m_buttons(buttons)
{
}

ButtonsModel::ButtonsModel(QObject *parent)
    : ButtonsModel(QVector<DecorationButtonType>({DecorationButtonType::Menu,
                                                  DecorationButtonType::ApplicationMenu,
                                                  DecorationButtonType::OnAllDesktops,
                                                  DecorationButtonType::Minimize,
                                                  DecorationButtonType::Maximize,
                                                  DecorationButtonType::Close,
                                                  DecorationButtonType::ContextHelp,
                                                  DecorationButtonType::Shade,
                                                  DecorationButtonType::KeepBelow,
                                                  DecorationButtonType::KeepAbove}),
                   parent)
{
}

ButtonsModel::~ButtonsModel() = default;

int ButtonsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_buttons.count();
}

static QString buttonToName(DecorationButtonType type)
{
    switch (type) {
    case DecorationButtonType::Menu:
        return i18n("More actions for this window");
    case DecorationButtonType::ApplicationMenu:
        return i18n("Application menu");
    case DecorationButtonType::OnAllDesktops:
        return i18n("On all desktops");
    case DecorationButtonType::Minimize:
        return i18n("Minimize");
    case DecorationButtonType::Maximize:
        return i18n("Maximize");
    case DecorationButtonType::Close:
        return i18n("Close");
    case DecorationButtonType::ContextHelp:
        return i18n("Context help");
    case DecorationButtonType::Shade:
        return i18n("Shade");
    case DecorationButtonType::KeepBelow:
        return i18n("Keep below other windows");
    case DecorationButtonType::KeepAbove:
        return i18n("Keep above other windows");
    default:
        return QString();
    }
}

QVariant ButtonsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_buttons.count() || index.column() != 0) {
        return QVariant();
    }
    switch (role) {
    case Qt::DisplayRole:
        return buttonToName(m_buttons.at(index.row()));
    case Qt::UserRole:
        return QVariant::fromValue(int(m_buttons.at(index.row())));
    }
    return QVariant();
}

QHash<int, QByteArray> ButtonsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(Qt::DisplayRole, QByteArrayLiteral("display"));
    roles.insert(Qt::UserRole, QByteArrayLiteral("button"));
    return roles;
}

void ButtonsModel::remove(int row)
{
    if (row < 0 || row >= m_buttons.count()) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_buttons.removeAt(row);
    endRemoveRows();
}

void ButtonsModel::down(int index)
{
    if (m_buttons.count() < 2 || index == m_buttons.count() - 1) {
        return;
    }
    beginMoveRows(QModelIndex(), index, index, QModelIndex(), index + 2);
    m_buttons.insert(index + 1, m_buttons.takeAt(index));
    endMoveRows();
}

void ButtonsModel::up(int index)
{
    if (m_buttons.count() < 2 || index == 0) {
        return;
    }
    beginMoveRows(QModelIndex(), index, index, QModelIndex(), index - 1);
    m_buttons.insert(index - 1, m_buttons.takeAt(index));
    endMoveRows();
}

void ButtonsModel::add(DecorationButtonType type)
{
    beginInsertRows(QModelIndex(), m_buttons.count(), m_buttons.count());
    m_buttons.append(type);
    endInsertRows();
}

void ButtonsModel::add(int index, int type)
{
    beginInsertRows(QModelIndex(), index, index);
    m_buttons.insert(index, KDecoration2::DecorationButtonType(type));
    endInsertRows();
}

void ButtonsModel::move(int sourceIndex, int targetIndex)
{
    if (sourceIndex == std::max(0, targetIndex)) {
        return;
    }

    /* When moving an item down, the destination index needs to be incremented
       by one, as explained in the documentation:
       https://doc.qt.io/qt-5/qabstractitemmodel.html#beginMoveRows */
    if (targetIndex > sourceIndex) {
        // Row will be moved down
        beginMoveRows(QModelIndex(), sourceIndex, sourceIndex, QModelIndex(), targetIndex + 1);
    } else {
        beginMoveRows(QModelIndex(), sourceIndex, sourceIndex, QModelIndex(), std::max(0, targetIndex));
    }

    m_buttons.move(sourceIndex, std::max(0, targetIndex));
    endMoveRows();
}

void ButtonsModel::clear()
{
    beginResetModel();
    m_buttons.clear();
    endResetModel();
}

void ButtonsModel::replace(const QVector<DecorationButtonType> &buttons)
{
    if (buttons.isEmpty()) {
        return;
    }

    beginResetModel();
    m_buttons = buttons;
    endResetModel();
}

}
}
