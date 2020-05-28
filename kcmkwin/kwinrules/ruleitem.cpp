/*
 * Copyright (c) 2020 Ismael Asensio <isma.af@gmail.com>
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
    , m_policy(new RulePolicy(policyType))
    , m_options(nullptr)
{
    reset();
}

RuleItem::~RuleItem()
{
    delete m_policy;
    delete m_options;
}

void RuleItem::reset()
{
    m_enabled = hasFlag(AlwaysEnabled) | hasFlag(StartEnabled);
    m_value = typedValue(QVariant(), m_type);
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
    m_value = typedValue(value, m_type);
}

QVariant RuleItem::suggestedValue() const
{
    return m_suggestedValue;
}

void RuleItem::setSuggestedValue(QVariant value, bool forceValue)
{
    if (forceValue) {
        setValue(value);
    }
    m_suggestedValue = value.isNull() ? QVariant() : typedValue(value, m_type);
}

QVariant RuleItem::options() const
{
    if (!m_options) {
        return QVariant();
    }
    return QVariant::fromValue(m_options);
}

void RuleItem::setOptionsData(const QList<OptionsModel::Data> &data)
{
    if (!m_options) {
        if (m_type != Option && m_type != FlagsOption) {
            return;
        }
        m_options = new OptionsModel();
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
    return QVariant::fromValue(m_policy);
}

QString RuleItem::policyKey() const
{
    return m_policy->policyKey(m_key);
}

QVariant RuleItem::typedValue(const QVariant &value, const RuleItem::Type type)
{
    switch (type) {
        case Undefined:
        case Option:
            return value;
        case Boolean:
            return value.toBool();
        case Integer:
        case Percentage:
            return value.toInt();
        case FlagsOption:
            // HACK: Currently, the only user of this is "types" property
            if (value.toInt() == -1) { //NET:AllTypesMask
                return 0x3FF - 0x040;  //All possible flags minus NET::Override (deprecated)
            }
            return value.toInt();
        case Point:
            return value.toPoint();
        case Size:
            return value.toSize();
        case String:
            return value.toString().trimmed();
        case Shortcut:
            return value.toString();
    }
    return value;
}

}   //namespace

