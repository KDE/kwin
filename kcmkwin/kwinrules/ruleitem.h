/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KWIN_RULEITEM_H
#define KWIN_RULEITEM_H

#include "optionsmodel.h"

#include <QFlag>
#include <QIcon>


namespace KWin
{

class RuleItem : public QObject
{
    Q_OBJECT

public:
    enum Type {
        Undefined,
        Boolean,
        String,
        Integer,
        Option,
        NetTypes,
        Percentage,
        Point,
        Size,
        Shortcut
    };
    Q_ENUM(Type)

    enum Flags {
        NoFlags            = 0,
        AlwaysEnabled      = 1u << 0,
        StartEnabled       = 1u << 1,
        AffectsWarning     = 1u << 2,
        AffectsDescription = 1u << 3,
        SuggestionOnly     = 1u << 4,
        AllFlags           = 0b11111
    };

public:
    RuleItem() {};
    RuleItem(const QString &key,
             const RulePolicy::Type policyType,
             const Type type,
             const QString &name,
             const QString &section,
             const QIcon &icon = QIcon::fromTheme("window"),
             const QString &description = QString("")
            );
    ~RuleItem();

    QString key() const;
    QString name() const;
    QString section() const;
    QIcon icon() const;
    QString iconName() const;
    QString description() const;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool hasFlag(RuleItem::Flags flag) const;
    void setFlag(RuleItem::Flags flag, bool active=true);

    Type type() const;
    QVariant value() const;
    void setValue(QVariant value);
    QVariant suggestedValue() const;
    void setSuggestedValue(QVariant value, bool forceValue = false);

    QVariant options() const;
    void setOptionsData(const QList<OptionsModel::Data> &data);
    uint optionsMask() const;

    RulePolicy::Type policyType() const;
    int policy() const;          // int belongs to anonymous enum in Rules::
    void setPolicy(int policy);  // int belongs to anonymous enum in Rules::
    QVariant policyModel() const;
    QString policyKey() const;

    void reset();

private:
    QVariant typedValue(const QVariant &value) const;

private:
    QString m_key;
    RuleItem::Type m_type;
    QString m_name;
    QString m_section;
    QIcon m_icon;
    QString m_description;
    QFlags<Flags> m_flags;

    bool m_enabled;

    QVariant m_value;
    QVariant m_suggestedValue;

    RulePolicy *m_policy;
    OptionsModel *m_options;
    uint m_optionsMask;
};

}   //namespace

#endif  //KWIN_RULEITEM_H
