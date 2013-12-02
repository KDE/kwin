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
    Q_PROPERTY(int animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationSpeedChanged)
    Q_PROPERTY(int windowThumbnail READ windowThumbnail WRITE setWindowThumbnail NOTIFY windowThumbnailChanged)
    Q_PROPERTY(int glScaleFilter READ glScaleFilter WRITE setGlScaleFilter NOTIFY glScaleFilterChanged)
    Q_PROPERTY(bool xrScaleFilter READ xrScaleFilter WRITE setXrScaleFilter NOTIFY xrScaleFilterChanged)
    Q_PROPERTY(bool unredirectFullscreen READ unredirectFullscreen WRITE setUnredirectFullscreen NOTIFY unredirectFullscreenChanged)
    Q_PROPERTY(int glSwapStrategy READ glSwapStrategy WRITE setGlSwapStrategy NOTIFY glSwapStrategyChanged)
    Q_PROPERTY(bool glColorCorrection READ glColorCorrection WRITE setGlColorCorrection NOTIFY glColorCorrectionChanged)
public:
    explicit Compositing(QObject *parent = 0);

    Q_INVOKABLE bool OpenGLIsUnsafe() const;
    Q_INVOKABLE bool OpenGLIsBroken();
    int animationSpeed() const;
    int windowThumbnail() const;
    int glScaleFilter() const;
    bool xrScaleFilter() const;
    bool unredirectFullscreen() const;
    int glSwapStrategy() const;
    bool glColorCorrection() const;

    void setAnimationSpeed(int speed);
    void setWindowThumbnail(int index);
    void setGlScaleFilter(int index);
    void setXrScaleFilter(bool filter);
    void setUnredirectFullscreen(bool unredirect);
    void setGlSwapStrategy(int strategy);
    void setGlColorCorrection(bool correction);

    void save();

public Q_SLOTS:
    void reset();

Q_SIGNALS:
    void changed();
    void animationSpeedChanged();
    void windowThumbnailChanged();
    void glScaleFilterChanged();
    void xrScaleFilterChanged();
    void unredirectFullscreenChanged();
    void glSwapStrategyChanged();
    void glColorCorrectionChanged();

private:
    int m_animationSpeed;
    int m_windowThumbnail;
    int m_glScaleFilter;
    bool m_xrScaleFilter;
    bool m_unredirectFullscreen;
    int m_glSwapStrategy;
    bool m_glColorCorrection;
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
