/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "previewsettings.h"
#include "buttonsmodel.h"
#include "previewbridge.h"

#include <KLocalizedString>

#include <QFontDatabase>

namespace KDecoration3
{

namespace Preview
{

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

QHash<int, QByteArray> BorderSizesModel::roleNames() const
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
    , m_leftButtons(new ButtonsModel(QList<DecorationButtonType>({DecorationButtonType::Menu,
                                                                  DecorationButtonType::ApplicationMenu,
                                                                  DecorationButtonType::OnAllDesktops}),
                                     this))
    , m_rightButtons(new ButtonsModel(QList<DecorationButtonType>({DecorationButtonType::ContextHelp,
                                                                   DecorationButtonType::Minimize,
                                                                   DecorationButtonType::Maximize,
                                                                   DecorationButtonType::Close}),
                                      this))
    , m_availableButtons(new ButtonsModel(QList<DecorationButtonType>({DecorationButtonType::Menu,
                                                                       DecorationButtonType::ApplicationMenu,
                                                                       DecorationButtonType::OnAllDesktops,
                                                                       DecorationButtonType::Minimize,
                                                                       DecorationButtonType::Maximize,
                                                                       DecorationButtonType::Close,
                                                                       DecorationButtonType::ContextHelp,
                                                                       DecorationButtonType::Shade,
                                                                       DecorationButtonType::KeepBelow,
                                                                       DecorationButtonType::KeepAbove}),
                                          this))
    , m_borderSizes(new BorderSizesModel(this))
    , m_borderSize(int(BorderSize::Normal))
    , m_font(QFontDatabase::systemFont(QFontDatabase::TitleFont))
{
    connect(this, &PreviewSettings::alphaChannelSupportedChanged, parent, &DecorationSettings::alphaChannelSupportedChanged);
    connect(this, &PreviewSettings::onAllDesktopsAvailableChanged, parent, &DecorationSettings::onAllDesktopsAvailableChanged);
    connect(this, &PreviewSettings::closeOnDoubleClickOnMenuChanged, parent, &DecorationSettings::closeOnDoubleClickOnMenuChanged);
    connect(this, &PreviewSettings::fontChanged, parent, &DecorationSettings::fontChanged);
    auto updateLeft = [this, parent]() {
        Q_EMIT parent->decorationButtonsLeftChanged(decorationButtonsLeft());
    };
    auto updateRight = [this, parent]() {
        Q_EMIT parent->decorationButtonsRightChanged(decorationButtonsRight());
    };
    connect(m_leftButtons, &QAbstractItemModel::rowsRemoved, this, updateLeft);
    connect(m_leftButtons, &QAbstractItemModel::rowsMoved, this, updateLeft);
    connect(m_leftButtons, &QAbstractItemModel::rowsInserted, this, updateLeft);
    connect(m_rightButtons, &QAbstractItemModel::rowsRemoved, this, updateRight);
    connect(m_rightButtons, &QAbstractItemModel::rowsMoved, this, updateRight);
    connect(m_rightButtons, &QAbstractItemModel::rowsInserted, this, updateRight);
}

PreviewSettings::~PreviewSettings() = default;

QAbstractItemModel *PreviewSettings::availableButtonsModel() const
{
    return m_availableButtons;
}

QAbstractItemModel *PreviewSettings::leftButtonsModel() const
{
    return m_leftButtons;
}

QAbstractItemModel *PreviewSettings::rightButtonsModel() const
{
    return m_rightButtons;
}

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
    Q_EMIT alphaChannelSupportedChanged(m_alphaChannelSupported);
}

void PreviewSettings::setOnAllDesktopsAvailable(bool available)
{
    if (m_onAllDesktopsAvailable == available) {
        return;
    }
    m_onAllDesktopsAvailable = available;
    Q_EMIT onAllDesktopsAvailableChanged(m_onAllDesktopsAvailable);
}

void PreviewSettings::setCloseOnDoubleClickOnMenu(bool enabled)
{
    if (m_closeOnDoubleClick == enabled) {
        return;
    }
    m_closeOnDoubleClick = enabled;
    Q_EMIT closeOnDoubleClickOnMenuChanged(enabled);
}

QList<DecorationButtonType> PreviewSettings::decorationButtonsLeft() const
{
    return m_leftButtons->buttons();
}

QList<DecorationButtonType> PreviewSettings::decorationButtonsRight() const
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
    Q_EMIT borderSizesIndexChanged(index);
    Q_EMIT decorationSettings()->borderSizeChanged(borderSize());
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
    Q_EMIT fontChanged(m_font);
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
    Q_EMIT bridgeChanged();
}

PreviewBridge *Settings::bridge() const
{
    return m_bridge.data();
}

void Settings::createSettings()
{
    if (m_bridge.isNull()) {
        m_settings.reset();
    } else {
        m_settings = std::make_shared<KDecoration3::DecorationSettings>(m_bridge.get());
        m_previewSettings = m_bridge->lastCreatedSettings();
        m_previewSettings->setBorderSizesIndex(m_borderSize);
        connect(this, &Settings::borderSizesIndexChanged, m_previewSettings, &PreviewSettings::setBorderSizesIndex);
    }
    Q_EMIT settingsChanged();
}

std::shared_ptr<DecorationSettings> Settings::settings() const
{
    return m_settings;
}

DecorationSettings *Settings::settingsPointer() const
{
    return m_settings.get();
}

void Settings::setBorderSizesIndex(int index)
{
    if (m_borderSize == index) {
        return;
    }
    m_borderSize = index;
    Q_EMIT borderSizesIndexChanged(m_borderSize);
}

}
}

#include "moc_previewsettings.cpp"
