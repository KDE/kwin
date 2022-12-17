/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ruleitem.h"
#include <rules.h>
#include <rulesettings.h>
#include <virtualdesktopsdbustypes.h>

#include <QAbstractListModel>
#include <QObject>
#include <QSortFilterProxyModel>

#if KWIN_BUILD_ACTIVITIES
#include <KActivities/Consumer>
#endif

namespace KWin
{

class RulesModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QStringList warningMessages READ warningMessages NOTIFY warningMessagesChanged)

public:
    enum RulesRole {
        NameRole = Qt::DisplayRole,
        DescriptionRole = Qt::ToolTipRole,
        IconRole = Qt::DecorationRole,
        IconNameRole = Qt::UserRole + 1,
        KeyRole,
        SectionRole,
        EnabledRole,
        SelectableRole,
        ValueRole,
        TypeRole,
        PolicyRole,
        PolicyModelRole,
        OptionsModelRole,
        SuggestedValueRole
    };
    Q_ENUM(RulesRole)

public:
    explicit RulesModel(QObject *parent = nullptr);
    ~RulesModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    QModelIndex indexOf(const QString &key) const;
    bool hasRule(const QString &key) const;
    RuleItem *ruleItem(const QString &key) const;

    RuleSettings *settings() const;
    void setSettings(RuleSettings *settings);

    void setSuggestedProperties(const QVariantMap &info);

    QString description() const;
    void setDescription(const QString &description);
    QStringList warningMessages() const;

    Q_INVOKABLE void detectWindowProperties(int miliseconds);

Q_SIGNALS:
    void descriptionChanged();
    void warningMessagesChanged();

    void showSuggestions();
    void showErrorMessage(const QString &title, const QString &message);

    void virtualDesktopsUpdated();

private:
    void populateRuleList();
    RuleItem *addRule(RuleItem *rule);
    void writeToSettings(RuleItem *rule);

    QString defaultDescription() const;
    void processSuggestion(const QString &key, const QVariant &value);

    bool wmclassWarning() const;
    bool geometryWarning() const;

    static const QHash<QString, QString> x11PropertyHash();
    void updateVirtualDesktops();

    QList<OptionsModel::Data> windowTypesModelData() const;
    QList<OptionsModel::Data> virtualDesktopsModelData() const;
    QList<OptionsModel::Data> activitiesModelData() const;
    QList<OptionsModel::Data> placementModelData() const;
    QList<OptionsModel::Data> focusModelData() const;
    QList<OptionsModel::Data> colorSchemesModelData() const;

private Q_SLOTS:
    void selectX11Window();

private:
    QList<RuleItem *> m_ruleList;
    QHash<QString, RuleItem *> m_rules;
    DBusDesktopDataVector m_virtualDesktops;
#if KWIN_BUILD_ACTIVITIES
    KActivities::Consumer *m_activities;
#endif
    RuleSettings *m_settings;
};

}
