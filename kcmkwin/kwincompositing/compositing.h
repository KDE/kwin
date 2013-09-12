/**************************************************************************
 * KWin - the KDE window manager                                          *
 * This file is part of the KDE project.                                  *
 *                                                                        *
 * Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 2 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/


#ifndef COMPOSITING_H
#define COMPOSITING_H

#include <QAbstractItemModel>
#include <QObject>

namespace KWin {
namespace Compositing {

class Compositing : public QObject
{

    Q_OBJECT
    Q_PROPERTY(int animationSpeed READ animationSpeed CONSTANT);
    Q_PROPERTY(int windowThumbnail READ windowThumbnail CONSTANT);
    Q_PROPERTY(int glSclaleFilter READ glSclaleFilter CONSTANT);
    Q_PROPERTY(bool xrSclaleFilter READ xrSclaleFilter CONSTANT);
    Q_PROPERTY(bool unredirectFullscreen READ unredirectFullscreen CONSTANT);
    Q_PROPERTY(int glSwapStrategy READ glSwapStrategy CONSTANT);
    Q_PROPERTY(bool glColorCorrection READ glColorCorrection CONSTANT);
public:
    explicit Compositing(QObject *parent = 0);

    Q_INVOKABLE bool OpenGLIsUnsafe() const;
    Q_INVOKABLE bool OpenGLIsBroken();
    int animationSpeed() const;
    int windowThumbnail() const;
    int glSclaleFilter() const;
    bool xrSclaleFilter() const;
    bool unredirectFullscreen() const;
    int glSwapStrategy() const;
    bool glColorCorrection() const;

private:

    enum OpenGLTypeIndex {
        OPENGL31_INDEX = 0,
        OPENGL20_INDEX,
        OPENGL12_INDEX,
        XRENDER_INDEX
    };
};


struct CompositingData;

class CompositingType : public QAbstractItemModel
{

    Q_OBJECT

public:

    enum CompositingTypeIndex {
        OPENGL31_INDEX = 0,
        OPENGL20_INDEX,
        OPENGL12_INDEX,
        XRENDER_INDEX
    };

    enum CompositingTypeRoles {
        NameRole = Qt::UserRole +1,
    };

    explicit CompositingType(QObject *parent = 0);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual QHash< int, QByteArray > roleNames() const override;

    Q_INVOKABLE int currentOpenGLType();
    Q_INVOKABLE void syncConfig(int openGLType, int animationSpeed, int windowThumbnail, int glSclaleFilter, bool xrSclaleFilter,
    bool unredirectFullscreen, int glSwapStrategy, bool glColorCorrection);

private:
    void generateCompositing();
    QList<CompositingData> m_compositingList;

};

struct CompositingData {
    QString name;
    CompositingType::CompositingTypeIndex type;
};


}//end namespace Compositing
}//end namespace KWin
#endif
