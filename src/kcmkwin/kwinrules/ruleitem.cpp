/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "ruleitem.h"

namespace KWin
{

RuleItem::RuleItem(const QString &key,
                   const RulePolicy::Type policyType,
                   const RuleItem::Type type,
                   const QString &name,
                   const QString &section,
                   const QIcon &icon,
                   const QString &description)
    : m_key(key)
    , m_type(type)
    , m_name(name)
    , m_section(section)
    , m_icon(icon)
    , m_description(description)
    , m_flags(NoFlags)
    , m_enabled(false)
    , m_policy(std::make_unique<RulePolicy>(policyType))
{
    reset();
}

void RuleItem::reset()
{
    m_enabled = hasFlag(AlwaysEnabled) || hasFlag(StartEnabled);
    m_value = typedValue(QVariant());
    m_suggestedValue = QVariant();
    m_policy->resetValue();
    if (m_options) {
        m_options->resetValue();
    }
}

QString RuleItem::key() const
{
    return m_key;
}

QString RuleItem::name() const
{
    return m_name;
}

QString RuleItem::section() const
{
    return m_section;
}

QString RuleItem::iconName() const
{
    return m_icon.name();
}

QIcon RuleItem::icon() const
{
    return m_icon;
}

QString RuleItem::description() const
{
    return m_description;
}

bool RuleItem::isEnabled() const
{
    return m_enabled;
}

void RuleItem::setEnabled(bool enabled)
{
    m_enabled = (enabled && !hasFlag(SuggestionOnly)) || hasFlag(AlwaysEnabled);
}

bool RuleItem::hasFlag(RuleItem::Flags flag) const
{
    return m_flags.testFlag(flag);
}

void RuleItem::setFlag(RuleItem::Flags flag, bool active)
{
    m_flags.setFlag(flag, active);
}

RuleItem::Type RuleItem::type() const
{
    return m_type;
}

QVariant RuleItem::value() const
{
    if (m_options && m_type == Option) {
        return m_options->value();
    }
    return m_value;
}

void RuleItem::setValue(QVariant value)
{
    if (m_options && m_type == Option) {
        m_options->setValue(value);
    }
    m_value = typedValue(value);
}

QVariant RuleItem::suggestedValue() const
{
    return m_suggestedValue;
}

void RuleItem::setSuggestedValue(QVariant value)
{
    m_suggestedValue = value.isNull() ? QVariant() : typedValue(value);
}

QVariant RuleItem::options() const
{
    if (!m_options) {
        return QVariant();
    }
    return QVariant::fromValue(m_options.get());
}

void RuleItem::setOptionsData(const QList<OptionsModel::Data> &data)
{
    if (m_type != Option && m_type != OptionList && m_type != NetTypes) {
        return;
    }
    if (!m_options) {
        m_options = std::make_unique<OptionsModel>(QList<OptionsModel::Data>{}, m_type == NetTypes);
    }
    m_options->updateModelData(data);
    m_options->setValue(m_value);
}

int RuleItem::policy() const
{
    return m_policy->value();
}

void RuleItem::setPolicy(int policy)
{
    m_policy->setValue(policy);
}

RulePolicy::Type RuleItem::policyType() const
{
    return m_policy->type();
}

QVariant RuleItem::policyModel() const
{
    return QVariant::fromValue(m_policy.get());
}

QString RuleItem::policyKey() const
{
    return m_policy->policyKey(m_key);
}

QVariant RuleItem::typedValue(const QVariant &value) const
{
    switch (type()) {
    case Undefined:
    case Option:
        return value;
    case Boolean:
        return value.toBool();
    case Integer:
    case Percentage:
        return value.toInt();
    case NetTypes: {
        const uint typesMask = m_options ? value.toUInt() & m_options->allOptionsMask() : 0; // filter by the allowed mask in the model
        if (typesMask == 0 || typesMask == m_options->allOptionsMask()) { // if no types or all of them are selected
            return 0U - 1; // return an all active mask (NET:AllTypesMask)
        }
        return typesMask;
    }
    case Point: {
        const QPoint point = value.toPoint();
        return (point == invalidPoint) ? QPoint(0, 0) : point;
    }
    case Size:
        return value.toSize();
    case String:
        if (value.type() == QVariant::StringList && !value.toStringList().isEmpty()) {
            return value.toStringList().at(0).trimmed();
        }
        return value.toString().trimmed();
    case Shortcut:
        return value.toString();
    case OptionList:
        return value.toStringList();
    }
    return value;
}

} // namespace
