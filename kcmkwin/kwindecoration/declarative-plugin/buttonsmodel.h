/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#ifndef KDECOARTIONS_PREVIEW_BUTTONS_MODEL_H
#define KDECOARTIONS_PREVIEW_BUTTONS_MODEL_H

#include <KDecoration2/DecorationButton>
#include <QAbstractListModel>

namespace KDecoration2
{

namespace Preview
{
class PreviewBridge;

class ButtonsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ButtonsModel(const QVector< DecorationButtonType > &buttons, QObject *parent = nullptr);
    explicit ButtonsModel(QObject *parent = nullptr);
    ~ButtonsModel() override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QHash< int, QByteArray > roleNames() const override;

    QVector< DecorationButtonType > buttons() const {
        return m_buttons;
    }

    Q_INVOKABLE void clear();
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void up(int index);
    Q_INVOKABLE void down(int index);
    Q_INVOKABLE void move(int sourceIndex, int targetIndex);

    void replace(const QVector< DecorationButtonType > &buttons);
    void add(DecorationButtonType type);
    Q_INVOKABLE void add(int index, int type);

private:
    QVector< DecorationButtonType > m_buttons;
};

}
}

#endif

