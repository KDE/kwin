/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "optionsmodel.h"

#include <KLocalizedString>


namespace KWin
{

QHash<int, QByteArray> OptionsModel::roleNames() const
{
    return {
        {Qt::DisplayRole,    QByteArrayLiteral("display")},
        {Qt::DecorationRole, QByteArrayLiteral("decoration")},
        {Qt::ToolTipRole,    QByteArrayLiteral("tooltip")},
        {Qt::UserRole,       QByteArrayLiteral("value")},
        {Qt::UserRole + 1,   QByteArrayLiteral("iconName")},
    };
}

int OptionsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_data.size();
}

QVariant OptionsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }

    const Data data = m_data.at(index.row());

    switch (role) {
        case Qt::DisplayRole:
            return data.text;
        case Qt::UserRole:
            return data.value;
        case Qt::DecorationRole:
            return data.icon;
        case Qt::UserRole + 1:
            return data.icon.name();
        case Qt::ToolTipRole:
            return data.description;
    }
    return QVariant();
}

int OptionsModel::selectedIndex() const
{
    return m_index;
}

int OptionsModel::indexOf(QVariant value) const
{
    for (int index = 0; index < m_data.count(); index++) {
        if (m_data.at(index).value == value) {
            return index;
        }
    }
    return -1;
}

QString OptionsModel::textOfValue(QVariant value) const
{
    int index = indexOf(value);
    if (index < 0 || index >= m_data.count()) {
        return QString();
    }
    return m_data.at(index).text;
}

QVariant OptionsModel::value() const
{
    if (m_data.isEmpty()) {
        return QVariant();
    }
    return m_data.at(m_index).value;
}

void OptionsModel::setValue(QVariant value)
{
    if (this->value() == value) {
        return;
    }
    int index = indexOf(value);
    if (index >= 0 && index != m_index) {
        m_index = index;
        emit selectedIndexChanged(index);
    }
}

void OptionsModel::resetValue()
{
    m_index = 0;
    emit selectedIndexChanged(m_index);
}

void OptionsModel::updateModelData(const QList<Data> &data) {
    beginResetModel();
    m_data = data;
    endResetModel();
}


RulePolicy::Type RulePolicy::type() const
{
    return m_type;
}

int RulePolicy::value() const
{
    if (m_type == RulePolicy::NoPolicy) {
        return Rules::Apply;   // To simplify external checks when rule has no policy
    }
    return OptionsModel::value().toInt();
}

QString RulePolicy::policyKey(const QString &key) const
{
    switch (m_type) {
        case NoPolicy:
            return QString();
        case StringMatch:
            return QStringLiteral("%1match").arg(key);
        case SetRule:
        case ForceRule:
            return QStringLiteral("%1rule").arg(key);
    }

    return QString();
}

QList<RulePolicy::Data> RulePolicy::policyOptions(RulePolicy::Type type)
{
    static const auto stringMatchOptions = QList<RulePolicy::Data> {
        {Rules::UnimportantMatch, i18n("Unimportant")},
        {Rules::ExactMatch,       i18n("Exact Match")},
        {Rules::SubstringMatch,   i18n("Substring Match")},
        {Rules::RegExpMatch,      i18n("Regular Expression")}
    };

    static const auto setRuleOptions = QList<RulePolicy::Data> {
        {Rules::DontAffect,
            i18n("Do Not Affect"),
            i18n("The window property will not be affected and therefore the default handling for it will be used."
                 "\nSpecifying this will block more generic window settings from taking effect.")},
        {Rules::Apply,
            i18n("Apply Initially"),
            i18n("The window property will be only set to the given value after the window is created."
                 "\nNo further changes will be affected.")},
        {Rules::Remember,
            i18n("Remember"),
            i18n("The value of the window property will be remembered and, every time the window"
                 " is created, the last remembered value will be applied.")},
        {Rules::Force,
            i18n("Force"),
            i18n("The window property will be always forced to the given value.")},
        {Rules::ApplyNow,
            i18n("Apply Now"),
            i18n("The window property will be set to the given value immediately and will not be affected later"
                 "\n(this action will be deleted afterwards).")},
        {Rules::ForceTemporarily,
            i18n("Force Temporarily"),
            i18n("The window property will be forced to the given value until it is hidden"
                 "\n(this action will be deleted after the window is hidden).")}
    };

    static auto forceRuleOptions = QList<RulePolicy::Data> {
        setRuleOptions.at(0),  // Rules::DontAffect
        setRuleOptions.at(3),  // Rules::Force
        setRuleOptions.at(5),  // Rules::ForceTemporarily
    };

    switch (type) {
    case NoPolicy:
        return {};
    case StringMatch:
        return stringMatchOptions;
    case SetRule:
        return setRuleOptions;
    case ForceRule:
        return forceRuleOptions;
    }
    return {};
}

}   //namespace
