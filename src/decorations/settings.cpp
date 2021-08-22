/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "settings.h"
// KWin
#include "decorationbridge.h"
#include "composite.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include "appmenu.h"

#include <config-kwin.h>

#include <KDecoration2/DecorationSettings>

#include <KConfigGroup>

#include <QFontDatabase>

#include <map>

namespace KWin
{
namespace Decoration
{
SettingsImpl::SettingsImpl(KDecoration2::DecorationSettings *parent)
    : QObject()
    , DecorationSettingsPrivate(parent)
    , m_borderSize(KDecoration2::BorderSize::Normal)
{
    readSettings();

    auto c = connect(Compositor::self(), &Compositor::compositingToggled,
            parent, &KDecoration2::DecorationSettings::alphaChannelSupportedChanged);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::countChanged, this,
        [parent](uint previous, uint current) {
            if (previous != 1 && current != 1) {
                return;
            }
            Q_EMIT parent->onAllDesktopsAvailableChanged(current > 1);
        }
    );
    // prevent changes in Decoration due to Compositor being destroyed
    connect(Compositor::self(), &Compositor::aboutToDestroy, this,
        [c] { disconnect(c); }
    );
    connect(Workspace::self(), &Workspace::configChanged, this, &SettingsImpl::readSettings);
    connect(DecorationBridge::self(), &DecorationBridge::metaDataLoaded, this, &SettingsImpl::readSettings);
}

SettingsImpl::~SettingsImpl() = default;

bool SettingsImpl::isAlphaChannelSupported() const
{
    return Compositor::self()->compositing();
}

bool SettingsImpl::isOnAllDesktopsAvailable() const
{
    return VirtualDesktopManager::self()->count() > 1;
}

bool SettingsImpl::isCloseOnDoubleClickOnMenu() const
{
    return m_closeDoubleClickMenu;
}

using ButtonsMap = std::map<KDecoration2::DecorationButtonType, QChar>;

static ButtonsMap decorationButtons()
{
    using ButtonType = KDecoration2::DecorationButtonType;

    static const ButtonsMap map = {
        {ButtonType::Menu, QChar('M')},
        {ButtonType::ApplicationMenu, QChar('N')},
        {ButtonType::OnAllDesktops, QChar('S')},
        {ButtonType::ContextHelp, QChar('H')},
        {ButtonType::Minimize, QChar('I')},
        {ButtonType::Maximize, QChar('A')},
        {ButtonType::Close, QChar('X')},
        {ButtonType::KeepAbove, QChar('F')},
        {ButtonType::KeepBelow, QChar('B')},
        {ButtonType::Shade, QChar('L')},
    };

    return map;
}

static QString buttonsToString(const QVector<KDecoration2::DecorationButtonType> &buttons)
{
    const ButtonsMap &map = decorationButtons();

    auto buttonToString = [&map](KDecoration2::DecorationButtonType button) -> QChar {
        const auto it = map.find(button);
        if (it != map.cend()) {
            return it->second;
        }
        return QChar();
    };

    QString ret;
    for (auto button : buttons) {
        ret.append(buttonToString(button));
    }
    return ret;
}

QVector< KDecoration2::DecorationButtonType > SettingsImpl::readDecorationButtons(const KConfigGroup &config,
                                                                                    const char *key,
                                                                                    const QVector< KDecoration2::DecorationButtonType > &defaultValue) const
{
    const ButtonsMap &btnMap = decorationButtons();

    auto buttonsFromString = [&btnMap](const QString &buttons) -> QVector<KDecoration2::DecorationButtonType> {
        QVector<KDecoration2::DecorationButtonType> ret;
        for (auto it = buttons.begin(); it != buttons.end(); ++it) {
            for (const auto [buttonType, associatedChar] : btnMap) {
                if (associatedChar == (*it)) {
                    ret << buttonType;
                }
            }
        }
        return ret;
    };

    return buttonsFromString(config.readEntry(key, buttonsToString(defaultValue)));
}

static KDecoration2::BorderSize stringToSize(const QString &name)
{
    static const QMap<QString, KDecoration2::BorderSize> s_sizes = QMap<QString, KDecoration2::BorderSize>({
        {QStringLiteral("None"), KDecoration2::BorderSize::None},
        {QStringLiteral("NoSides"), KDecoration2::BorderSize::NoSides},
        {QStringLiteral("Tiny"), KDecoration2::BorderSize::Tiny},
        {QStringLiteral("Normal"), KDecoration2::BorderSize::Normal},
        {QStringLiteral("Large"), KDecoration2::BorderSize::Large},
        {QStringLiteral("VeryLarge"), KDecoration2::BorderSize::VeryLarge},
        {QStringLiteral("Huge"), KDecoration2::BorderSize::Huge},
        {QStringLiteral("VeryHuge"), KDecoration2::BorderSize::VeryHuge},
        {QStringLiteral("Oversized"), KDecoration2::BorderSize::Oversized}
    });
    auto it = s_sizes.constFind(name);
    if (it == s_sizes.constEnd()) {
        // non sense values are interpreted just like normal
        return KDecoration2::BorderSize::Normal;
    }
    return it.value();
}

void SettingsImpl::readSettings()
{
    KConfigGroup config = kwinApp()->config()->group(QStringLiteral("org.kde.kdecoration2"));
    const auto &left = readDecorationButtons(config, "ButtonsOnLeft", QVector<KDecoration2::DecorationButtonType >({
        KDecoration2::DecorationButtonType::Menu,
        KDecoration2::DecorationButtonType::OnAllDesktops
    }));
    if (left != m_leftButtons) {
        m_leftButtons = left;
        Q_EMIT decorationSettings()->decorationButtonsLeftChanged(m_leftButtons);
    }
    const auto &right = readDecorationButtons(config, "ButtonsOnRight", QVector<KDecoration2::DecorationButtonType >({
        KDecoration2::DecorationButtonType::ContextHelp,
        KDecoration2::DecorationButtonType::Minimize,
        KDecoration2::DecorationButtonType::Maximize,
        KDecoration2::DecorationButtonType::Close
    }));
    if (right != m_rightButtons) {
        m_rightButtons = right;
        Q_EMIT decorationSettings()->decorationButtonsRightChanged(m_rightButtons);
    }
    ApplicationMenu::self()->setViewEnabled(left.contains(KDecoration2::DecorationButtonType::ApplicationMenu) || right.contains(KDecoration2::DecorationButtonType::ApplicationMenu));
    const bool close = config.readEntry("CloseOnDoubleClickOnMenu", false);
    if (close != m_closeDoubleClickMenu) {
        m_closeDoubleClickMenu = close;
        Q_EMIT decorationSettings()->closeOnDoubleClickOnMenuChanged(m_closeDoubleClickMenu);
    }
    m_autoBorderSize = config.readEntry("BorderSizeAuto", true);

    auto size = stringToSize(config.readEntry("BorderSize", QStringLiteral("Normal")));
    if (m_autoBorderSize) {
        /* Falls back to Normal border size, if the plugin does not provide a valid recommendation. */
        size = stringToSize(DecorationBridge::self()->recommendedBorderSize());
    }
    if (size != m_borderSize) {
        m_borderSize = size;
        Q_EMIT decorationSettings()->borderSizeChanged(m_borderSize);
    }
    const QFont font = QFontDatabase::systemFont(QFontDatabase::TitleFont);
    if (font != m_font) {
        m_font = font;
        Q_EMIT decorationSettings()->fontChanged(m_font);
    }

    Q_EMIT decorationSettings()->reconfigured();
}

}
}
