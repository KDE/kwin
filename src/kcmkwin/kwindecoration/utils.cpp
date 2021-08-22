/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "utils.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <map>

namespace Utils
{
using BorderSizeMap = QMap<KDecoration2::BorderSize, QString>;

const BorderSizeMap &getBorderSizeNames()
{
    static const BorderSizeMap borderSizeNames {
        { KDecoration2::BorderSize::None,      i18n("No Borders") },
        { KDecoration2::BorderSize::NoSides,   i18n("No Side Borders") },
        { KDecoration2::BorderSize::Tiny,      i18n("Tiny") },
        { KDecoration2::BorderSize::Normal,    i18n("Normal") },
        { KDecoration2::BorderSize::Large,     i18n("Large") },
        { KDecoration2::BorderSize::VeryLarge, i18n("Very Large") },
        { KDecoration2::BorderSize::Huge,      i18n("Huge") },
        { KDecoration2::BorderSize::VeryHuge,  i18n("Very Huge") },
        { KDecoration2::BorderSize::Oversized, i18n("Oversized") }
    };

    return borderSizeNames;
}

using ButtonsMap = std::map<KDecoration2::DecorationButtonType, QChar>;

static ButtonsMap decorationButtons()
{
    using ButtonType = KDecoration2::DecorationButtonType;

    static const ButtonsMap map {
        {ButtonType::Menu, QChar('M') },
        {ButtonType::ApplicationMenu, QChar('N') },
        {ButtonType::OnAllDesktops, QChar('S') },
        {ButtonType::ContextHelp, QChar('H') },
        {ButtonType::Minimize, QChar('I') },
        {ButtonType::Maximize, QChar('A') },
        {ButtonType::Close, QChar('X') },
        {ButtonType::KeepAbove, QChar('F') },
        {ButtonType::KeepBelow, QChar('B') },
        {ButtonType::Shade, QChar('L') }
    };

    return map;
}

QString buttonsToString(const DecorationButtonsList &buttons)
{
    const auto &btnMap = decorationButtons();

    auto buttonToString = [&btnMap](KDecoration2::DecorationButtonType button) -> QChar {
        const auto it = btnMap.find(button);
        return it != btnMap.cend() ? it->second : QChar();
    };

    QString ret;
    for (auto button : buttons) {
        ret.append(buttonToString(button));
    }
    return ret;
}

DecorationButtonsList buttonsFromString(const QString &buttons)
{
    const auto &btnMap = decorationButtons();

    DecorationButtonsList ret;
    for (auto it = buttons.begin(); it != buttons.end(); ++it) {
        for (const auto [buttonType, associatedChar] : btnMap) {
            if (associatedChar == *it) {
                ret << buttonType;
            }
        }
    }
    return ret;
}

DecorationButtonsList readDecorationButtons(const KConfigGroup &config, const QString &key, const DecorationButtonsList &defaultValue)
{
    return buttonsFromString(config.readEntry(key, buttonsToString(defaultValue)));
}

KDecoration2::BorderSize stringToBorderSize(const QString &name)
{
    const BorderSizeMap &map = getBorderSizeNames();

    for (auto it = map.cbegin(), end = map.cend(); it != end; ++it) {
        if (name == it.value()) {
            return it.key();
        }
    }

    // Non-sense values are interpreted just like normal
    return KDecoration2::BorderSize::Normal;
}

QString borderSizeToString(KDecoration2::BorderSize size)
{
    return getBorderSizeNames().value(size);
}

} // namespace Utils
