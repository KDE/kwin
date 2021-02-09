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
            emit parent->onAllDesktopsAvailableChanged(current > 1);
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

static QHash<KDecoration2::DecorationButtonType, QChar> s_buttonNames;
static void initButtons()
{
    if (!s_buttonNames.isEmpty()) {
        return;
    }
    s_buttonNames[KDecoration2::DecorationButtonType::Menu]            = QChar('M');
    s_buttonNames[KDecoration2::DecorationButtonType::ApplicationMenu] = QChar('N');
    s_buttonNames[KDecoration2::DecorationButtonType::OnAllDesktops]   = QChar('S');
    s_buttonNames[KDecoration2::DecorationButtonType::ContextHelp]     = QChar('H');
    s_buttonNames[KDecoration2::DecorationButtonType::Minimize]        = QChar('I');
    s_buttonNames[KDecoration2::DecorationButtonType::Maximize]        = QChar('A');
    s_buttonNames[KDecoration2::DecorationButtonType::Close]           = QChar('X');
    s_buttonNames[KDecoration2::DecorationButtonType::KeepAbove]       = QChar('F');
    s_buttonNames[KDecoration2::DecorationButtonType::KeepBelow]       = QChar('B');
    s_buttonNames[KDecoration2::DecorationButtonType::Shade]           = QChar('L');
}

static QString buttonsToString(const QVector<KDecoration2::DecorationButtonType> &buttons)
{
    auto buttonToString = [](KDecoration2::DecorationButtonType button) -> QChar {
        const auto it = s_buttonNames.constFind(button);
        if (it != s_buttonNames.constEnd()) {
            return it.value();
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
    initButtons();
    auto buttonsFromString = [](const QString &buttons) -> QVector<KDecoration2::DecorationButtonType> {
        QVector<KDecoration2::DecorationButtonType> ret;
        for (auto it = buttons.begin(); it != buttons.end(); ++it) {
            for (auto it2 = s_buttonNames.constBegin(); it2 != s_buttonNames.constEnd(); ++it2) {
                if (it2.value() == (*it)) {
                    ret << it2.key();
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
        emit decorationSettings()->decorationButtonsLeftChanged(m_leftButtons);
    }
    const auto &right = readDecorationButtons(config, "ButtonsOnRight", QVector<KDecoration2::DecorationButtonType >({
        KDecoration2::DecorationButtonType::ContextHelp,
        KDecoration2::DecorationButtonType::Minimize,
        KDecoration2::DecorationButtonType::Maximize,
        KDecoration2::DecorationButtonType::Close
    }));
    if (right != m_rightButtons) {
        m_rightButtons = right;
        emit decorationSettings()->decorationButtonsRightChanged(m_rightButtons);
    }
    ApplicationMenu::self()->setViewEnabled(left.contains(KDecoration2::DecorationButtonType::ApplicationMenu) || right.contains(KDecoration2::DecorationButtonType::ApplicationMenu));
    const bool close = config.readEntry("CloseOnDoubleClickOnMenu", false);
    if (close != m_closeDoubleClickMenu) {
        m_closeDoubleClickMenu = close;
        emit decorationSettings()->closeOnDoubleClickOnMenuChanged(m_closeDoubleClickMenu);
    }
    m_autoBorderSize = config.readEntry("BorderSizeAuto", true);

    auto size = stringToSize(config.readEntry("BorderSize", QStringLiteral("Normal")));
    if (m_autoBorderSize) {
        /* Falls back to Normal border size, if the plugin does not provide a valid recommendation. */
        size = stringToSize(DecorationBridge::self()->recommendedBorderSize());
    }
    if (size != m_borderSize) {
        m_borderSize = size;
        emit decorationSettings()->borderSizeChanged(m_borderSize);
    }
    const QFont font = QFontDatabase::systemFont(QFontDatabase::TitleFont);
    if (font != m_font) {
        m_font = font;
        emit decorationSettings()->fontChanged(m_font);
    }

    emit decorationSettings()->reconfigured();
}

}
}
