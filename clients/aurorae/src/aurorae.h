/********************************************************************
Copyright (C) 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef AURORAE_H
#define AURORAE_H

#include "themeconfig.h"

#include <kdecoration.h>
#include <kdecorationfactory.h>

class QDeclarativeComponent;
class QDeclarativeEngine;
class QDeclarativeItem;
class QGraphicsSceneMouseEvent;
class QGraphicsScene;
class QGraphicsView;
class KConfig;
class KConfigGroup;

namespace Aurorae
{
class AuroraeTheme;
class AuroraeClient;

class AuroraeFactory :  public QObject, public KDecorationFactoryUnstable
{
    Q_OBJECT
    Q_PROPERTY(QString leftButtons READ leftButtons NOTIFY buttonsChanged)
    Q_PROPERTY(QString rightButtons READ rightButtons NOTIFY buttonsChanged)
    Q_PROPERTY(bool customButtonPositions READ customButtonPositions NOTIFY buttonsChanged)
    Q_PROPERTY(QFont activeTitleFont READ activeTitleFont NOTIFY titleFontChanged)
    Q_PROPERTY(QFont inactiveTitleFont READ inactiveTitleFont NOTIFY titleFontChanged)
public:
    ~AuroraeFactory();

    static AuroraeFactory* instance();
    bool reset(unsigned long changed);
    KDecoration *createDecoration(KDecorationBridge*);
    bool supports(Ability ability) const;
    virtual QList< BorderSize > borderSizes() const;

    AuroraeTheme *theme() const {
        return m_theme;
    }
    QDeclarativeItem *createQmlDecoration(AuroraeClient *client);
    QString leftButtons();
    QString rightButtons();
    bool customButtonPositions();

    QFont activeTitleFont();
    QFont inactiveTitleFont();

private:
    enum EngineType {
        AuroraeEngine,
        QMLEngine
    };
    AuroraeFactory();
    void init();
    void initAurorae(KConfig &conf, KConfigGroup &group);
    void initQML(const KConfigGroup& group);

Q_SIGNALS:
    void buttonsChanged();
    void titleFontChanged();

private:
    static AuroraeFactory *s_instance;

    AuroraeTheme *m_theme;
    QDeclarativeEngine *m_engine;
    QDeclarativeComponent *m_component;
    EngineType m_engineType;
};

class AuroraeClient : public KDecorationUnstable
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QString caption READ caption NOTIFY captionChanged)
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)
    Q_PROPERTY(QRect geometry READ geometry)
    Q_PROPERTY(int height READ height)
    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(bool closeable READ isCloseable CONSTANT)
    Q_PROPERTY(bool maximizeable READ isMaximizable CONSTANT)
    Q_PROPERTY(bool minimizeable READ isMinimizable CONSTANT)
    Q_PROPERTY(bool modal READ isModal)
    Q_PROPERTY(bool moveable READ isMovable CONSTANT)
    Q_PROPERTY(bool onAllDesktops READ isOnAllDesktops NOTIFY desktopChanged)
    Q_PROPERTY(bool preview READ isPreview CONSTANT)
    Q_PROPERTY(bool resizeable READ isResizable CONSTANT)
    Q_PROPERTY(bool setShade READ isSetShade NOTIFY shadeChanged)
    Q_PROPERTY(bool shade READ isShade WRITE setShade NOTIFY shadeChanged)
    Q_PROPERTY(bool shadeable READ isShadeable)
    Q_PROPERTY(bool keepAbove READ keepAbove WRITE setKeepAbove NOTIFY keepAboveChangedWrapper)
    Q_PROPERTY(bool keepBelow READ keepBelow WRITE setKeepBelow NOTIFY keepBelowChangedWrapper)
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizeChanged)
    Q_PROPERTY(bool providesContextHelp READ providesContextHelp)
    Q_PROPERTY(QRect transparentRect READ transparentRect)
    Q_PROPERTY(int width READ width)
    Q_PROPERTY(qulonglong windowId READ windowId CONSTANT)
    Q_PROPERTY(int doubleClickInterval READ doubleClickInterval)
    // TODO: window tabs - they suck for dynamic features
public:
    AuroraeClient(KDecorationBridge* bridge, KDecorationFactory* factory);
    virtual ~AuroraeClient();
    virtual bool eventFilter(QObject *object, QEvent *event);
    virtual void activeChange();
    virtual void borders(int& left, int& right, int& top, int& bottom) const;
    virtual void captionChange();
    virtual void desktopChange();
    virtual void iconChange();
    virtual void init();
    virtual void maximizeChange();
    virtual QSize minimumSize() const;
    virtual Position mousePosition(const QPoint& p) const;
    virtual void resize(const QSize& s);
    virtual void shadeChange();
    // optional overrides
    virtual void padding(int &left, int &right, int &top, int &bottom) const;
    virtual void reset(long unsigned int changed);
    bool isMaximized() const;
    int doubleClickInterval() const;

Q_SIGNALS:
    void activeChanged();
    void captionChanged();
    void desktopChanged();
    void iconChanged();
    void maximizeChanged();
    void shadeChanged();
    void keepAboveChangedWrapper();
    void keepBelowChangedWrapper();

public slots:
    void menuClicked();
    void toggleShade();
    void toggleKeepAbove();
    void toggleKeepBelow();
    void titlePressed(int button, int buttons);
    void titleReleased(int button, int buttons);
    void titleMouseMoved(int button, int buttons);
    void titlePressed(Qt::MouseButton button, Qt::MouseButtons buttons);
    void titleReleased(Qt::MouseButton button, Qt::MouseButtons buttons);
    void titleMouseMoved(Qt::MouseButton button, Qt::MouseButtons buttons);
    void closeWindow();
    void titlebarDblClickOperation();
    void maximize(int button);

private slots:
    void themeChanged();
    void doCloseWindow();
    void doTitlebarDblClickOperation();
    void doMaximzie(int button);

private:
    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QDeclarativeItem *m_item;
};

}

#endif
