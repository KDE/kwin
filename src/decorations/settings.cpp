/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "config-kwin.h"

#include "appmenu.h"
#include "compositor.h"
#include "decorationbridge.h"
#include "settings.h"
#include "virtualdesktops.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KDecoration3/DecorationSettings>

#include <QFontDatabase>

namespace KWin
{
namespace Decoration
{
SettingsImpl::SettingsImpl(KDecoration3::DecorationSettings *parent)
    : QObject()
    , DecorationSettingsPrivate(parent)
    , m_borderSize(KDecoration3::BorderSize::Normal)
{
    readSettings();

    auto c = connect(Compositor::self(), &Compositor::compositingToggled,
                     parent, &KDecoration3::DecorationSettings::alphaChannelSupportedChanged);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::countChanged, this, [parent](uint previous, uint current) {
        if (previous != 1 && current != 1) {
            return;
        }
        Q_EMIT parent->onAllDesktopsAvailableChanged(current > 1);
    });
    // prevent changes in Decoration due to Compositor being destroyed
    connect(Compositor::self(), &Compositor::aboutToDestroy, this, [c]() {
        disconnect(c);
    });
    connect(Workspace::self(), &Workspace::configChanged, this, &SettingsImpl::readSettings);
    connect(Workspace::self()->decorationBridge(), &DecorationBridge::metaDataLoaded, this, &SettingsImpl::readSettings);
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

static QHash<KDecoration3::DecorationButtonType, QChar> s_buttonNames;
static void initButtons()
{
    if (!s_buttonNames.isEmpty()) {
        return;
    }
    s_buttonNames[KDecoration3::DecorationButtonType::Menu] = QChar('M');
    s_buttonNames[KDecoration3::DecorationButtonType::ApplicationMenu] = QChar('N');
    s_buttonNames[KDecoration3::DecorationButtonType::OnAllDesktops] = QChar('S');
    s_buttonNames[KDecoration3::DecorationButtonType::ContextHelp] = QChar('H');
    s_buttonNames[KDecoration3::DecorationButtonType::Minimize] = QChar('I');
    s_buttonNames[KDecoration3::DecorationButtonType::Maximize] = QChar('A');
    s_buttonNames[KDecoration3::DecorationButtonType::Close] = QChar('X');
    s_buttonNames[KDecoration3::DecorationButtonType::KeepAbove] = QChar('F');
    s_buttonNames[KDecoration3::DecorationButtonType::KeepBelow] = QChar('B');
    s_buttonNames[KDecoration3::DecorationButtonType::Shade] = QChar('L');
    s_buttonNames[KDecoration3::DecorationButtonType::Spacer] = QChar('_');
}

static QString buttonsToString(const QList<KDecoration3::DecorationButtonType> &buttons)
{
    auto buttonToString = [](KDecoration3::DecorationButtonType button) -> QChar {
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

QList<KDecoration3::DecorationButtonType> SettingsImpl::readDecorationButtons(const KConfigGroup &config,
                                                                              const char *key,
                                                                              const QList<KDecoration3::DecorationButtonType> &defaultValue) const
{
    initButtons();
    auto buttonsFromString = [](const QString &buttons) -> QList<KDecoration3::DecorationButtonType> {
        QList<KDecoration3::DecorationButtonType> ret;
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

static KDecoration3::BorderSize stringToSize(const QString &name)
{
    static const QMap<QString, KDecoration3::BorderSize> s_sizes = QMap<QString, KDecoration3::BorderSize>({{QStringLiteral("None"), KDecoration3::BorderSize::None},
                                                                                                            {QStringLiteral("NoSides"), KDecoration3::BorderSize::NoSides},
                                                                                                            {QStringLiteral("Tiny"), KDecoration3::BorderSize::Tiny},
                                                                                                            {QStringLiteral("Normal"), KDecoration3::BorderSize::Normal},
                                                                                                            {QStringLiteral("Large"), KDecoration3::BorderSize::Large},
                                                                                                            {QStringLiteral("VeryLarge"), KDecoration3::BorderSize::VeryLarge},
                                                                                                            {QStringLiteral("Huge"), KDecoration3::BorderSize::Huge},
                                                                                                            {QStringLiteral("VeryHuge"), KDecoration3::BorderSize::VeryHuge},
                                                                                                            {QStringLiteral("Oversized"), KDecoration3::BorderSize::Oversized}});
    auto it = s_sizes.constFind(name);
    if (it == s_sizes.constEnd()) {
        // non sense values are interpreted just like normal
        return KDecoration3::BorderSize::Normal;
    }
    return it.value();
}

void SettingsImpl::readSettings()
{
    KConfigGroup config = kwinApp()->config()->group(QStringLiteral("org.kde.kdecoration2"));
    const auto &left = readDecorationButtons(config, "ButtonsOnLeft", QList<KDecoration3::DecorationButtonType>({KDecoration3::DecorationButtonType::Menu, KDecoration3::DecorationButtonType::OnAllDesktops}));
    if (left != m_leftButtons) {
        m_leftButtons = left;
        Q_EMIT decorationSettings()->decorationButtonsLeftChanged(m_leftButtons);
    }
    const auto &right = readDecorationButtons(config, "ButtonsOnRight", QList<KDecoration3::DecorationButtonType>({KDecoration3::DecorationButtonType::ContextHelp, KDecoration3::DecorationButtonType::Minimize, KDecoration3::DecorationButtonType::Maximize, KDecoration3::DecorationButtonType::Close}));
    if (right != m_rightButtons) {
        m_rightButtons = right;
        Q_EMIT decorationSettings()->decorationButtonsRightChanged(m_rightButtons);
    }
    Workspace::self()->applicationMenu()->setViewEnabled(left.contains(KDecoration3::DecorationButtonType::ApplicationMenu) || right.contains(KDecoration3::DecorationButtonType::ApplicationMenu));
    const bool close = config.readEntry("CloseOnDoubleClickOnMenu", false);
    if (close != m_closeDoubleClickMenu) {
        m_closeDoubleClickMenu = close;
        Q_EMIT decorationSettings()->closeOnDoubleClickOnMenuChanged(m_closeDoubleClickMenu);
    }
    m_autoBorderSize = config.readEntry("BorderSizeAuto", true);

    auto size = stringToSize(config.readEntry("BorderSize", QStringLiteral("Normal")));
    if (m_autoBorderSize) {
        /* Falls back to Normal border size, if the plugin does not provide a valid recommendation. */
        size = stringToSize(Workspace::self()->decorationBridge()->recommendedBorderSize());
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

#include "moc_settings.cpp"
