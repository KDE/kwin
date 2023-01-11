/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "decorationoptions.h"
#include <KConfigGroup>
#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationSettings>
#include <KSharedConfig>
#include <QGuiApplication>
#include <QStyleHints>

namespace KWin
{

ColorSettings::ColorSettings(const QPalette &pal)
{
    init(pal);
}

void ColorSettings::update(const QPalette &pal)
{
    init(pal);
}

void ColorSettings::init(const QPalette &pal)
{
    m_palette = pal;
    KConfigGroup wmConfig(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), QStringLiteral("WM"));
    m_activeFrameColor = wmConfig.readEntry("frame", pal.color(QPalette::Active, QPalette::Window));
    m_inactiveFrameColor = wmConfig.readEntry("inactiveFrame", m_activeFrameColor);
    m_activeTitleBarColor = wmConfig.readEntry("activeBackground", pal.color(QPalette::Active, QPalette::Highlight));
    m_inactiveTitleBarColor = wmConfig.readEntry("inactiveBackground", m_inactiveFrameColor);
    m_activeTitleBarBlendColor = wmConfig.readEntry("activeBlend", m_activeTitleBarColor.darker(110));
    m_inactiveTitleBarBlendColor = wmConfig.readEntry("inactiveBlend", m_inactiveTitleBarColor.darker(110));
    m_activeFontColor = wmConfig.readEntry("activeForeground", pal.color(QPalette::Active, QPalette::HighlightedText));
    m_inactiveFontColor = wmConfig.readEntry("inactiveForeground", m_activeFontColor.darker());
    m_activeButtonColor = wmConfig.readEntry("activeTitleBtnBg", m_activeFrameColor.lighter(130));
    m_inactiveButtonColor = wmConfig.readEntry("inactiveTitleBtnBg", m_inactiveFrameColor.lighter(130));
    m_activeHandle = wmConfig.readEntry("handle", m_activeFrameColor);
    m_inactiveHandle = wmConfig.readEntry("inactiveHandle", m_activeHandle);
}

DecorationOptions::DecorationOptions(QObject *parent)
    : QObject(parent)
    , m_active(true)
    , m_decoration(nullptr)
    , m_colors(ColorSettings(QPalette()))
{
    connect(this, &DecorationOptions::decorationChanged, this, &DecorationOptions::slotActiveChanged);
    connect(this, &DecorationOptions::decorationChanged, this, &DecorationOptions::colorsChanged);
    connect(this, &DecorationOptions::decorationChanged, this, &DecorationOptions::fontChanged);
    connect(this, &DecorationOptions::decorationChanged, this, &DecorationOptions::titleButtonsChanged);
}

DecorationOptions::~DecorationOptions()
{
}

QColor DecorationOptions::borderColor() const
{
    return m_active ? m_colors.activeFrame() : m_colors.inactiveFrame();
}

QColor DecorationOptions::buttonColor() const
{
    return m_active ? m_colors.activeButtonColor() : m_colors.inactiveButtonColor();
}

QColor DecorationOptions::fontColor() const
{
    return m_active ? m_colors.activeFont() : m_colors.inactiveFont();
}

QColor DecorationOptions::resizeHandleColor() const
{
    return m_active ? m_colors.activeHandle() : m_colors.inactiveHandle();
}

QColor DecorationOptions::titleBarBlendColor() const
{
    return m_active ? m_colors.activeTitleBarBlendColor() : m_colors.inactiveTitleBarBlendColor();
}

QColor DecorationOptions::titleBarColor() const
{
    return m_active ? m_colors.activeTitleBarColor() : m_colors.inactiveTitleBarColor();
}

QFont DecorationOptions::titleFont() const
{
    return m_decoration ? m_decoration->settings()->font() : QFont();
}

static int decorationButton(KDecoration2::DecorationButtonType type)
{
    switch (type) {
    case KDecoration2::DecorationButtonType::Menu:
        return DecorationOptions::DecorationButtonMenu;
    case KDecoration2::DecorationButtonType::ApplicationMenu:
        return DecorationOptions::DecorationButtonApplicationMenu;
    case KDecoration2::DecorationButtonType::OnAllDesktops:
        return DecorationOptions::DecorationButtonOnAllDesktops;
    case KDecoration2::DecorationButtonType::Minimize:
        return DecorationOptions::DecorationButtonMinimize;
    case KDecoration2::DecorationButtonType::Maximize:
        return DecorationOptions::DecorationButtonMaximizeRestore;
    case KDecoration2::DecorationButtonType::Close:
        return DecorationOptions::DecorationButtonClose;
    case KDecoration2::DecorationButtonType::ContextHelp:
        return DecorationOptions::DecorationButtonQuickHelp;
    case KDecoration2::DecorationButtonType::Shade:
        return DecorationOptions::DecorationButtonShade;
    case KDecoration2::DecorationButtonType::KeepBelow:
        return DecorationOptions::DecorationButtonKeepBelow;
    case KDecoration2::DecorationButtonType::KeepAbove:
        return DecorationOptions::DecorationButtonKeepAbove;
    default:
        return DecorationOptions::DecorationButtonNone;
    }
}

QList<int> DecorationOptions::titleButtonsLeft() const
{
    QList<int> ret;
    if (m_decoration) {
        for (auto it : m_decoration->settings()->decorationButtonsLeft()) {
            ret << decorationButton(it);
        }
    }
    return ret;
}

QList<int> DecorationOptions::titleButtonsRight() const
{
    QList<int> ret;
    if (m_decoration) {
        for (auto it : m_decoration->settings()->decorationButtonsRight()) {
            ret << decorationButton(it);
        }
    }
    return ret;
}

KDecoration2::Decoration *DecorationOptions::decoration() const
{
    return m_decoration;
}

void DecorationOptions::setDecoration(KDecoration2::Decoration *decoration)
{
    if (m_decoration == decoration) {
        return;
    }
    if (m_decoration) {
        // disconnect from existing decoration
        disconnect(m_decoration->client().toStrongRef().data(), &KDecoration2::DecoratedClient::activeChanged, this, &DecorationOptions::slotActiveChanged);
        auto s = m_decoration->settings();
        disconnect(s.data(), &KDecoration2::DecorationSettings::fontChanged, this, &DecorationOptions::fontChanged);
        disconnect(s.data(), &KDecoration2::DecorationSettings::decorationButtonsLeftChanged, this, &DecorationOptions::titleButtonsChanged);
        disconnect(s.data(), &KDecoration2::DecorationSettings::decorationButtonsRightChanged, this, &DecorationOptions::titleButtonsChanged);
        disconnect(m_paletteConnection);
    }
    m_decoration = decoration;
    connect(m_decoration->client().toStrongRef().data(), &KDecoration2::DecoratedClient::activeChanged, this, &DecorationOptions::slotActiveChanged);
    m_paletteConnection = connect(m_decoration->client().toStrongRef().data(), &KDecoration2::DecoratedClient::paletteChanged, this, [this](const QPalette &pal) {
        m_colors.update(pal);
        Q_EMIT colorsChanged();
    });
    auto s = m_decoration->settings();
    connect(s.data(), &KDecoration2::DecorationSettings::fontChanged, this, &DecorationOptions::fontChanged);
    connect(s.data(), &KDecoration2::DecorationSettings::decorationButtonsLeftChanged, this, &DecorationOptions::titleButtonsChanged);
    connect(s.data(), &KDecoration2::DecorationSettings::decorationButtonsRightChanged, this, &DecorationOptions::titleButtonsChanged);
    Q_EMIT decorationChanged();
}

void DecorationOptions::slotActiveChanged()
{
    if (!m_decoration) {
        return;
    }
    if (m_active == m_decoration->client().toStrongRef().data()->isActive()) {
        return;
    }
    m_active = m_decoration->client().toStrongRef().data()->isActive();
    Q_EMIT colorsChanged();
    Q_EMIT fontChanged();
}

int DecorationOptions::mousePressAndHoldInterval() const
{
    return QGuiApplication::styleHints()->mousePressAndHoldInterval();
}

Borders::Borders(QObject *parent)
    : QObject(parent)
    , m_left(0)
    , m_right(0)
    , m_top(0)
    , m_bottom(0)
{
}

Borders::~Borders()
{
}

void Borders::setLeft(int left)
{
    if (m_left != left) {
        m_left = left;
        Q_EMIT leftChanged();
    }
}
void Borders::setRight(int right)
{
    if (m_right != right) {
        m_right = right;
        Q_EMIT rightChanged();
    }
}
void Borders::setTop(int top)
{
    if (m_top != top) {
        m_top = top;
        Q_EMIT topChanged();
    }
}
void Borders::setBottom(int bottom)
{
    if (m_bottom != bottom) {
        m_bottom = bottom;
        Q_EMIT bottomChanged();
    }
}

void Borders::setAllBorders(int border)
{
    setBorders(border);
    setTitle(border);
}

void Borders::setBorders(int border)
{
    setSideBorders(border);
    setBottom(border);
}

void Borders::setSideBorders(int border)
{
    setLeft(border);
    setRight(border);
}

void Borders::setTitle(int value)
{
    setTop(value);
}

Borders::operator QMargins() const
{
    return QMargins(m_left, m_top, m_right, m_bottom);
}

} // namespace
