/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "utils.h"

#include <KConfigGroup>
#include <KLocalizedString>

namespace
{
const QMap<QString, KDecoration3::BorderSize> s_borderSizes{
    {QStringLiteral("None"), KDecoration3::BorderSize::None},
    {QStringLiteral("NoSides"), KDecoration3::BorderSize::NoSides},
    {QStringLiteral("Tiny"), KDecoration3::BorderSize::Tiny},
    {QStringLiteral("Normal"), KDecoration3::BorderSize::Normal},
    {QStringLiteral("Large"), KDecoration3::BorderSize::Large},
    {QStringLiteral("VeryLarge"), KDecoration3::BorderSize::VeryLarge},
    {QStringLiteral("Huge"), KDecoration3::BorderSize::Huge},
    {QStringLiteral("VeryHuge"), KDecoration3::BorderSize::VeryHuge},
    {QStringLiteral("Oversized"), KDecoration3::BorderSize::Oversized}};
const QMap<KDecoration3::BorderSize, QString> s_borderSizeNames{
    {KDecoration3::BorderSize::None, i18n("No window borders")},
    {KDecoration3::BorderSize::NoSides, i18n("No side window borders")},
    {KDecoration3::BorderSize::Tiny, i18n("Tiny window borders")},
    {KDecoration3::BorderSize::Normal, i18n("Normal window borders")},
    {KDecoration3::BorderSize::Large, i18n("Large window borders")},
    {KDecoration3::BorderSize::VeryLarge, i18n("Very large window borders")},
    {KDecoration3::BorderSize::Huge, i18n("Huge window borders")},
    {KDecoration3::BorderSize::VeryHuge, i18n("Very huge window borders")},
    {KDecoration3::BorderSize::Oversized, i18n("Oversized window borders")}};

const QHash<KDecoration3::DecorationButtonType, QChar> s_buttonNames{
    {KDecoration3::DecorationButtonType::Menu, QChar('M')},
    {KDecoration3::DecorationButtonType::ApplicationMenu, QChar('N')},
    {KDecoration3::DecorationButtonType::OnAllDesktops, QChar('S')},
    {KDecoration3::DecorationButtonType::ContextHelp, QChar('H')},
    {KDecoration3::DecorationButtonType::Minimize, QChar('I')},
    {KDecoration3::DecorationButtonType::Maximize, QChar('A')},
    {KDecoration3::DecorationButtonType::Close, QChar('X')},
    {KDecoration3::DecorationButtonType::KeepAbove, QChar('F')},
    {KDecoration3::DecorationButtonType::KeepBelow, QChar('B')},
    {KDecoration3::DecorationButtonType::Shade, QChar('L')},
    {KDecoration3::DecorationButtonType::Spacer, QChar('_')},
};
}

namespace Utils
{

QString buttonsToString(const DecorationButtonsList &buttons)
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

DecorationButtonsList buttonsFromString(const QString &buttons)
{
    DecorationButtonsList ret;
    for (auto it = buttons.begin(); it != buttons.end(); ++it) {
        for (auto it2 = s_buttonNames.constBegin(); it2 != s_buttonNames.constEnd(); ++it2) {
            if (it2.value() == (*it)) {
                ret << it2.key();
            }
        }
    }
    return ret;
}

DecorationButtonsList readDecorationButtons(const KConfigGroup &config, const QString &key, const DecorationButtonsList &defaultValue)
{
    return buttonsFromString(config.readEntry(key, buttonsToString(defaultValue)));
}

KDecoration3::BorderSize stringToBorderSize(const QString &name)
{
    auto it = s_borderSizes.constFind(name);
    if (it == s_borderSizes.constEnd()) {
        // non sense values are interpreted just like normal
        return KDecoration3::BorderSize::Normal;
    }
    return it.value();
}

QString borderSizeToString(KDecoration3::BorderSize size)
{
    return s_borderSizes.key(size);
}

const QMap<KDecoration3::BorderSize, QString> &getBorderSizeNames()
{
    return s_borderSizeNames;
}

} // namespace Utils
