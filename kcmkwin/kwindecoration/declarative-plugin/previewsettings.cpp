/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "previewsettings.h"
#include "previewbridge.h"

#include <KLocalizedString>

#include <QFontDatabase>

namespace KDecoration2
{

namespace Preview
{

ButtonsModel::ButtonsModel(const QList< DecorationButtonType > &buttons, QObject *parent)
    : QAbstractListModel(parent)
    , m_buttons(buttons)
{
}

ButtonsModel::ButtonsModel(QObject* parent)
    : ButtonsModel(QList<DecorationButtonType>({
        DecorationButtonType::Menu,
        DecorationButtonType::ApplicationMenu,
        DecorationButtonType::OnAllDesktops,
        DecorationButtonType::Minimize,
        DecorationButtonType::Maximize,
        DecorationButtonType::Close,
        DecorationButtonType::QuickHelp,
        DecorationButtonType::Shade,
        DecorationButtonType::KeepBelow,
        DecorationButtonType::KeepAbove
    }), parent)
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
            return i18n("Menu");
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
        case DecorationButtonType::QuickHelp:
            return i18n("Quick help");
        case DecorationButtonType::Shade:
            return i18n("Shade");
        case DecorationButtonType::KeepBelow:
            return i18n("Keep below");
        case DecorationButtonType::KeepAbove:
            return i18n("Keep above");
        default:
            return QString();
    }
}

QVariant ButtonsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() ||
            index.row() < 0 ||
            index.row() >= m_buttons.count() ||
            index.column() != 0) {
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

QHash< int, QByteArray > ButtonsModel::roleNames() const
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
    if (m_buttons.count() < 2 || index == m_buttons.count() -1) {
        return;
    }
    beginMoveRows(QModelIndex(), index, index, QModelIndex(), index + 2);
    m_buttons.move(index, index + 1);
    endMoveRows();
}

void ButtonsModel::up(int index)
{
    if (m_buttons.count() < 2 || index == 0) {
        return;
    }
    beginMoveRows(QModelIndex(), index, index, QModelIndex(), index -1);
    m_buttons.move(index, index - 1);
    endMoveRows();
}

void ButtonsModel::add(DecorationButtonType type)
{
    beginInsertRows(QModelIndex(), m_buttons.count(), m_buttons.count());
    m_buttons.append(type);
    endInsertRows();
}

BorderSizesModel::BorderSizesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

BorderSizesModel::~BorderSizesModel() = default;

QVariant BorderSizesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_borders.count() || index.column() != 0) {
        return QVariant();
    }
    if (role != Qt::DisplayRole && role != Qt::UserRole) {
        return QVariant();
    }
    return QVariant::fromValue<BorderSize>(m_borders.at(index.row()));
}

int BorderSizesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_borders.count();
}

QHash< int, QByteArray > BorderSizesModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(Qt::DisplayRole, QByteArrayLiteral("display"));
    return roles;
}

PreviewSettings::PreviewSettings(DecorationSettings *parent)
    : QObject()
    , DecorationSettingsPrivate(parent)
    , m_alphaChannelSupported(true)
    , m_onAllDesktopsAvailable(true)
    , m_closeOnDoubleClick(false)
    , m_leftButtons(new ButtonsModel(QList<DecorationButtonType>({
            DecorationButtonType::Menu,
            DecorationButtonType::OnAllDesktops
        }), this))
    , m_rightButtons(new ButtonsModel(QList<DecorationButtonType>({
            DecorationButtonType::QuickHelp,
            DecorationButtonType::Minimize,
            DecorationButtonType::Maximize,
            DecorationButtonType::Close
        }), this))
    , m_availableButtons(new ButtonsModel(QList<DecorationButtonType>({
            DecorationButtonType::Menu,
            DecorationButtonType::ApplicationMenu,
            DecorationButtonType::OnAllDesktops,
            DecorationButtonType::Minimize,
            DecorationButtonType::Maximize,
            DecorationButtonType::Close,
            DecorationButtonType::QuickHelp,
            DecorationButtonType::Shade,
            DecorationButtonType::KeepBelow,
            DecorationButtonType::KeepAbove
        }), this))
    , m_borderSizes(new BorderSizesModel(this))
    , m_borderSize(int(BorderSize::Normal))
    , m_font(QFontDatabase::systemFont(QFontDatabase::TitleFont))
{
    connect(this, &PreviewSettings::alphaChannelSupportedChanged, parent, &DecorationSettings::alphaChannelSupportedChanged);
    connect(this, &PreviewSettings::onAllDesktopsAvailableChanged, parent, &DecorationSettings::onAllDesktopsAvailableChanged);
    connect(this, &PreviewSettings::closeOnDoubleClickOnMenuChanged, parent, &DecorationSettings::closeOnDoubleClickOnMenuChanged);
    connect(this, &PreviewSettings::fontChanged, parent, &DecorationSettings::fontChanged);
    auto updateLeft = [this, parent]() {
        parent->decorationButtonsLeftChanged(decorationButtonsLeft());
    };
    auto updateRight = [this, parent]() {
        parent->decorationButtonsRightChanged(decorationButtonsRight());
    };
    connect(m_leftButtons,  &QAbstractItemModel::rowsRemoved,  this, updateLeft);
    connect(m_leftButtons,  &QAbstractItemModel::rowsMoved,    this, updateLeft);
    connect(m_leftButtons,  &QAbstractItemModel::rowsInserted, this, updateLeft);
    connect(m_rightButtons, &QAbstractItemModel::rowsRemoved,  this, updateRight);
    connect(m_rightButtons, &QAbstractItemModel::rowsMoved,    this, updateRight);
    connect(m_rightButtons, &QAbstractItemModel::rowsInserted, this, updateRight);
}

PreviewSettings::~PreviewSettings() = default;

bool PreviewSettings::isAlphaChannelSupported() const
{
    return m_alphaChannelSupported;
}

bool PreviewSettings::isOnAllDesktopsAvailable() const
{
    return m_onAllDesktopsAvailable;
}

void PreviewSettings::setAlphaChannelSupported(bool supported)
{
    if (m_alphaChannelSupported == supported) {
        return;
    }
    m_alphaChannelSupported = supported;
    emit alphaChannelSupportedChanged(m_alphaChannelSupported);
}

void PreviewSettings::setOnAllDesktopsAvailable(bool available)
{
    if (m_onAllDesktopsAvailable == available) {
        return;
    }
    m_onAllDesktopsAvailable = available;
    emit onAllDesktopsAvailableChanged(m_onAllDesktopsAvailable);
}

void PreviewSettings::setCloseOnDoubleClickOnMenu(bool enabled)
{
    if (m_closeOnDoubleClick == enabled) {
        return;
    }
    m_closeOnDoubleClick = enabled;
    emit closeOnDoubleClickOnMenuChanged(enabled);
}

QList< DecorationButtonType > PreviewSettings::decorationButtonsLeft() const
{
    return m_leftButtons->buttons();
}

QList< DecorationButtonType > PreviewSettings::decorationButtonsRight() const
{
    return m_rightButtons->buttons();
}

void PreviewSettings::addButtonToLeft(int row)
{
    QModelIndex index = m_availableButtons->index(row);
    if (!index.isValid()) {
        return;
    }
    m_leftButtons->add(index.data(Qt::UserRole).value<DecorationButtonType>());
}

void PreviewSettings::addButtonToRight(int row)
{
    QModelIndex index = m_availableButtons->index(row);
    if (!index.isValid()) {
        return;
    }
    m_rightButtons->add(index.data(Qt::UserRole).value<DecorationButtonType>());
}

void PreviewSettings::setBorderSizesIndex(int index)
{
    if (m_borderSize == index) {
        return;
    }
    m_borderSize = index;
    emit borderSizesIndexChanged(index);
    emit decorationSettings()->borderSizeChanged(borderSize());
}

BorderSize PreviewSettings::borderSize() const
{
    return m_borderSizes->index(m_borderSize).data(Qt::UserRole).value<BorderSize>();
}

void PreviewSettings::setFont(const QFont &font)
{
    if (m_font == font) {
        return;
    }
    m_font = font;
    emit fontChanged(m_font);
}

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    connect(this, &Settings::bridgeChanged, this, &Settings::createSettings);
}

Settings::~Settings() = default;

void Settings::setBridge(PreviewBridge *bridge)
{
    if (m_bridge == bridge) {
        return;
    }
    m_bridge = bridge;
    emit bridgeChanged();
}

PreviewBridge *Settings::bridge() const
{
    return m_bridge.data();
}

void Settings::createSettings()
{
    if (m_bridge.isNull()) {
        m_settings.clear();
    } else {
        m_settings = QSharedPointer<KDecoration2::DecorationSettings>::create(m_bridge.data());
    }
    emit settingsChanged();
}

QSharedPointer<DecorationSettings> Settings::settings() const
{
    return m_settings;
}

DecorationSettings *Settings::settingsPointer() const
{
    return m_settings.data();
}

}
}
