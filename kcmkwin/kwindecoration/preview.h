/*
 *
 * Copyright (c) 2003 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef KWINDECORATION_PREVIEW_H
#define KWINDECORATION_PREVIEW_H

#include <QWidget>
//Added by qt3to4:
#include <QMouseEvent>
#include <QLabel>
#include <QResizeEvent>
#include <kdecoration.h>
#include <kdecorationbridge.h>
#include <kdecoration_plugins_p.h>

class QLabel;
class QTextDocument;

class KDecorationPreviewBridge;
class KDecorationPreviewOptions;

class KDecorationPreview
    : public QWidget
{
    Q_OBJECT
public:
    // Note: Windows can't be added or removed without making changes to
    //       the code, since parts of it assume there's just an active
    //       and an inactive window.
    enum Windows { Inactive = 0, Active, NumWindows };

    KDecorationPreview(QWidget* parent = NULL);
    virtual ~KDecorationPreview();
    bool recreateDecoration(KDecorationPlugins* plugin);
    void enablePreview();
    void disablePreview();
    void setPreviewMask(const QRegion&, int, bool);
    QRegion unobscuredRegion(bool, const QRegion&) const;
    QRect windowGeometry(bool) const;
    void setTempBorderSize(KDecorationPlugins* plugin, KDecorationDefines::BorderSize size);
    void setTempButtons(KDecorationPlugins* plugin, bool customEnabled, const QString &left, const QString &right);
    QPixmap preview(QTextDocument* document, QWidget* widget);
protected:
    virtual void paintEvent(QPaintEvent*);
    virtual void resizeEvent(QResizeEvent*);
private:
    void positionPreviews();
    KDecorationPreviewOptions* options;
    KDecorationPreviewBridge* bridge[NumWindows];
    KDecoration* deco[NumWindows];
    QLabel* no_preview;
    QRegion mask;
};

class KDecorationPreviewBridge
    : public KDecorationBridgeUnstable
{
public:
    KDecorationPreviewBridge(KDecorationPreview* preview, bool active);
    virtual bool isActive() const;
    virtual bool isCloseable() const;
    virtual bool isMaximizable() const;
    virtual MaximizeMode maximizeMode() const;
    virtual bool isMinimizable() const;
    virtual bool providesContextHelp() const;
    virtual int desktop() const;
    virtual bool isModal() const;
    virtual bool isShadeable() const;
    virtual bool isShade() const;
    virtual bool isSetShade() const;
    virtual bool keepAbove() const;
    virtual bool keepBelow() const;
    virtual bool isMovable() const;
    virtual bool isResizable() const;
    virtual NET::WindowType windowType(unsigned long supported_types) const;
    virtual QIcon icon() const;
    virtual QString caption() const;
    virtual void processMousePressEvent(QMouseEvent*);
    virtual void showWindowMenu(const QRect &);
    virtual void showWindowMenu(const QPoint &);
    virtual void performWindowOperation(WindowOperation);
    virtual void setMask(const QRegion&, int);
    virtual bool isPreview() const;
    virtual QRect geometry() const;
    virtual QRect iconGeometry() const;
    virtual QRegion unobscuredRegion(const QRegion& r) const;
    virtual WId windowId() const;
    virtual void closeWindow();
    virtual void maximize(MaximizeMode mode);
    virtual void minimize();
    virtual void showContextHelp();
    virtual void setDesktop(int desktop);
    virtual void titlebarDblClickOperation();
    virtual void titlebarMouseWheelOperation(int delta);
    virtual void setShade(bool set);
    virtual void setKeepAbove(bool);
    virtual void setKeepBelow(bool);
    virtual int currentDesktop() const;
    virtual QWidget* initialParentWidget() const;
    virtual Qt::WFlags initialWFlags() const;
    virtual void grabXServer(bool grab);

    virtual bool compositingActive() const;
    virtual QRect transparentRect() const;

    // Window tabbing
    virtual QString caption(int idx) const;
    virtual void closeTab(long id);
    virtual void closeTabGroup();
    virtual long currentTabId() const;
    virtual QIcon icon(int idx) const;
    virtual void setCurrentTab(long id);
    virtual void showWindowMenu(const QPoint &, long id);
    virtual void tab_A_before_B(long A, long B);
    virtual void tab_A_behind_B(long A, long B);
    virtual int tabCount() const;
    virtual long tabId(int idx) const;
    virtual void untab(long id, const QRect& newGeom);
    virtual WindowOperation buttonToWindowOperation(Qt::MouseButtons button);

private:
    KDecorationPreview* preview;
    bool active;
};

class KDecorationPreviewOptions
    : public KDecorationOptions
{
public:
    KDecorationPreviewOptions();
    virtual ~KDecorationPreviewOptions();
    virtual unsigned long updateSettings();

    void setCustomBorderSize(BorderSize size);
    void setCustomTitleButtonsEnabled(bool enabled);
    void setCustomTitleButtons(const QString &left, const QString &right);

private:
    BorderSize customBorderSize;
    bool customButtonsChanged;
    bool customButtons;
    QString customTitleButtonsLeft;
    QString customTitleButtonsRight;
};

class KDecorationPreviewPlugins
    : public KDecorationPlugins
{
public:
    KDecorationPreviewPlugins(const KSharedConfigPtr &cfg);
    virtual bool provides(Requirement);
};

inline KDecorationPreviewPlugins::KDecorationPreviewPlugins(const KSharedConfigPtr &cfg)
    : KDecorationPlugins(cfg)
{
}

#endif
