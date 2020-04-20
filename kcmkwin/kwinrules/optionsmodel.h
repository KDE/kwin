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
