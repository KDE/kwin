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

class QMutex;
class QOpenGLFramebufferObject;
class QQmlComponent;
class QQmlEngine;
class QQuickItem;
class QQuickWindow;
class KConfig;
class KConfigGroup;

namespace Aurorae
{
class AuroraeTheme;
class AuroraeClient;

class AuroraeFactory :  public KDecorationFactory
{
    Q_OBJECT
public:
    ~AuroraeFactory();

    static AuroraeFactory* instance();
    static QObject *createInstance(QWidget *, QObject *, const QList<QVariant> &);
    KDecoration *createDecoration(KDecorationBridge*);
    bool supports(Ability ability) const;
    virtual QList< BorderSize > borderSizes() const;

    AuroraeTheme *theme() const {
        return m_theme;
    }
    QQuickItem *createQmlDecoration(AuroraeClient *client);
    const QString &currentThemeName() const {
        return m_themeName;
    }

private:
    enum EngineType {
        AuroraeEngine,
        QMLEngine
    };
    explicit AuroraeFactory(QObject *parent = nullptr);
    void init();
    void initAurorae(KConfig &conf, KConfigGroup &group);
    void initQML(const KConfigGroup& group);

Q_SIGNALS:
    void buttonsChanged();
    void titleFontChanged();
    void configChanged();

private Q_SLOTS:
    void updateConfiguration();

private:
    static AuroraeFactory *s_instance;

    AuroraeTheme *m_theme;
    QQmlEngine *m_engine;
    QQmlComponent *m_component;
    EngineType m_engineType;
    QString m_themeName;
};

class AuroraeClient : public KDecoration
{
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry)
    Q_PROPERTY(int height READ height)
    Q_PROPERTY(bool closeable READ isCloseable CONSTANT)
    Q_PROPERTY(bool maximizeable READ isMaximizable CONSTANT)
    Q_PROPERTY(bool minimizeable READ isMinimizable CONSTANT)
    Q_PROPERTY(bool modal READ isModal)
    Q_PROPERTY(bool moveable READ isMovable CONSTANT)
    Q_PROPERTY(bool preview READ isPreview CONSTANT)
    Q_PROPERTY(bool resizeable READ isResizable CONSTANT)
    Q_PROPERTY(bool shadeable READ isShadeable)
    Q_PROPERTY(bool providesContextHelp READ providesContextHelp)
    Q_PROPERTY(bool appMenu READ menuAvailable NOTIFY appMenuAvailableChanged)
    Q_PROPERTY(QRect transparentRect READ transparentRect)
    Q_PROPERTY(int width READ width)
    Q_PROPERTY(qulonglong windowId READ windowId CONSTANT)
    Q_PROPERTY(int doubleClickInterval READ doubleClickInterval)
    Q_PROPERTY(bool animationsSupported READ animationsSupported CONSTANT)
    // TODO: window tabs - they suck for dynamic features
public:
    AuroraeClient(KDecorationBridge* bridge, KDecorationFactory* factory);
    virtual ~AuroraeClient();
    virtual bool eventFilter(QObject *object, QEvent *event);
    virtual void borders(int& left, int& right, int& top, int& bottom) const;
    virtual void init();
    virtual QSize minimumSize() const;
    virtual Position mousePosition(const QPoint& p) const;
    virtual void resize(const QSize& s);
    // optional overrides
    virtual void padding(int &left, int &right, int &top, int &bottom) const;
    int doubleClickInterval() const;

    bool animationsSupported() const;

    Q_INVOKABLE QVariant readConfig(const QString &key, const QVariant &defaultValue = QVariant());

    virtual void render(QPaintDevice *device, const QRegion &sourceRegion);

Q_SIGNALS:
    void buttonsChanged();
    /**
     * Signal emitted when the decoration's configuration might have changed.
     * A decoration could reload it's configuration when this signal is emitted.
     **/
    void configChanged();
    void fontChanged();
    void appMenuAvailableChanged();

public Q_SLOTS:
    void menuClicked();
    void appMenuClicked();
    void toggleShade();
    void toggleKeepAbove();
    void toggleKeepBelow();
    void titlePressed(int button, int buttons);
    void titlePressed(Qt::MouseButton button, Qt::MouseButtons buttons);
    void closeWindow();
    void titlebarDblClickOperation();
    void maximize(int button);

    QRegion region(KDecorationDefines::Region r);

private Q_SLOTS:
    void themeChanged();
    void doCloseWindow();
    void doTitlebarDblClickOperation();
    void doMaximzie(int button);
    void slotAlphaChanged();

private:
    void sizesFromBorders(const QObject *borders, int &left, int &right, int &top, int &bottom) const;
    QQuickWindow *m_view;
    QScopedPointer<QQuickItem> m_item;
    QScopedPointer<QOpenGLFramebufferObject> m_fbo;
    QImage m_buffer;
    QScopedPointer<QMutex> m_mutex;
};

}

#endif
