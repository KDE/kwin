/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KWIN_OPTIONS_MODEL_H
#define KWIN_OPTIONS_MODEL_H

#include <rules.h>

#include <QAbstractListModel>
#include <QIcon>
#include <QVariant>


namespace KWin {

class OptionsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectedIndexChanged)

public:
    struct Data {
        Data(const QVariant &value, const QString &text, const QIcon &icon = {}, const QString &description = {})
            : value(value)
            , text(text)
            , icon(icon)
            , description(description)
            {}
        Data(const QVariant &value, const QString &text, const QString &description)
            : value(value)
            , text(text)
            , description(description)
            {}

        QVariant value;
        QString text;
        QIcon icon;
        QString description;
    };

public:
    OptionsModel() : QAbstractListModel(), m_data(), m_index(0) {};
    OptionsModel(const QList<Data> &data) : QAbstractListModel(), m_data(data), m_index(0) {};

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QVariant value() const;
    void setValue(QVariant value);
    void resetValue();

    void updateModelData(const QList<Data> &data);

    Q_INVOKABLE int indexOf(QVariant value) const;
    Q_INVOKABLE QString textOfValue(QVariant value) const;
    int selectedIndex() const;

signals:
    void selectedIndexChanged(int index);

public:
    QList<Data> m_data;

protected:
    int m_index = 0;
};

class RulePolicy : public OptionsModel
{
public:
    enum Type {
        NoPolicy,
        StringMatch,
        SetRule,
        ForceRule
    };

public:
    RulePolicy(Type type)
        : OptionsModel(policyOptions(type))
        , m_type(type)
        {};

    Type type() const;
    int value() const;
    QString policyKey(const QString &key) const;

private:
    static QList<Data> policyOptions(RulePolicy::Type type);

private:
    Type m_type;
};

}   //namespace

#endif  //KWIN_OPTIONS_MODEL_H
