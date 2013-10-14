/*
 *
 * Copyright (c) 2003 Lubos Lunak <l.lunak@kde.org>
 * Copyright (c) 2013 Martin Gräßlin <mgraesslin@kde.org>
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
#include <QQuickPaintedItem>
#include <kdecoration.h>
#include <kdecorationbridge.h>
#include <kdecoration_plugins_p.h>

class KDecorationPreviewBridge;
class KDecorationPreviewOptions;
class QMouseEvent;

class PreviewItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QString library READ libraryName WRITE setLibraryName NOTIFY libraryChanged)
public:
    PreviewItem(QQuickItem *parent = nullptr);
    virtual ~PreviewItem();
    virtual void paint(QPainter *painter);

    void setLibraryName(const QString &library);
    const QString &libraryName() const {
        return m_libraryName;
    }
    void updateDecoration(KDecorationPreviewBridge *bridge);

Q_SIGNALS:
    void libraryChanged();

protected:
    virtual void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);
    virtual void hoverMoveEvent(QHoverEvent *event);

private Q_SLOTS:
    void loadDecorationPlugin();
    void recreateDecorations();

private:
    void updatePreview();
    void updateSize(const QSize &size, KDecoration *decoration, QImage &buffer);
    void render(QImage *image, KDecoration *decoration);
    void forwardMoveEvent(QHoverEvent *event, const QRect &geo, KDecoration *deco, QWidget **entered);
    QScopedPointer<KDecorationPlugins> m_plugins;
    QScopedPointer<KDecorationPreviewBridge> m_activeBridge;
    QScopedPointer<KDecorationPreviewBridge> m_inactiveBridge;
    KDecoration *m_activeDecoration;
    KDecoration *m_inactiveDecoration;
    QWidget *m_activeEntered;
    QWidget *m_inactiveEntered;
    QString m_libraryName;
    QImage m_activeBuffer;
    QImage m_inactiveBuffer;
};

class KDecorationPreview
    : public QWidget
{
    Q_OBJECT
public:
    // Note: Windows can't be added or removed without making changes to
    //       the code, since parts of it assume there's just an active
    //       and an inactive window.
    enum Windows { Inactive = 0, Active, NumWindows };

    explicit KDecorationPreview(QWidget* parent = NULL);
    virtual ~KDecorationPreview();
    void disablePreview();
    KDecorationFactory *factory() const;
    QRegion unobscuredRegion(bool, const QRegion&) const;
    QRect windowGeometry(bool) const;
private:
    KDecorationPreviewBridge* bridge[NumWindows];
    KDecoration* deco[NumWindows];
};

class KDecorationPreviewBridge
    : public KDecorationBridge
{
public:
    KDecorationPreviewBridge(KDecorationPreview* preview, bool active);
    KDecorationPreviewBridge(PreviewItem* preview, bool active);
    virtual bool isActive() const override;
    virtual bool isCloseable() const override;
    virtual bool isMaximizable() const override;
    virtual MaximizeMode maximizeMode() const override;
    virtual QuickTileMode quickTileMode() const override;
    virtual bool isMinimizable() const override;
    virtual bool providesContextHelp() const override;
    virtual int desktop() const override;
    virtual bool isModal() const override;
    virtual bool isShadeable() const override;
    virtual bool isShade() const override;
    virtual bool isSetShade() const override;
    virtual bool keepAbove() const override;
    virtual bool keepBelow() const override;
    virtual bool isMovable() const override;
    virtual bool isResizable() const override;
    virtual NET::WindowType windowType(unsigned long supported_types) const override;
    virtual QIcon icon() const override;
    virtual QString caption() const override;
    virtual void processMousePressEvent(QMouseEvent*) override;
    virtual void showWindowMenu(const QRect &) override;
    virtual void showWindowMenu(const QPoint &) override;
    virtual void showApplicationMenu(const QPoint &) override;
    virtual bool menuAvailable() const override;
    virtual void performWindowOperation(WindowOperation) override;
    virtual void setMask(const QRegion&, int) override;
    virtual bool isPreview() const override;
    virtual QRect geometry() const override;
    virtual QRect iconGeometry() const override;
    virtual QRegion unobscuredRegion(const QRegion& r) const override;
    virtual WId windowId() const override;
    virtual void closeWindow() override;
    virtual void maximize(MaximizeMode mode) override;
    virtual void minimize() override;
    virtual void showContextHelp() override;
    virtual void setDesktop(int desktop) override;
    virtual void titlebarDblClickOperation() override;
    virtual void titlebarMouseWheelOperation(int delta) override;
    virtual void setShade(bool set) override;
    virtual void setKeepAbove(bool) override;
    virtual void setKeepBelow(bool) override;
    virtual int currentDesktop() const override;
    virtual Qt::WindowFlags initialWFlags() const override;
    virtual void grabXServer(bool grab) override;

    virtual bool compositingActive() const override;
    virtual QRect transparentRect() const override;

    virtual void update(const QRegion &region) override;
    virtual QPalette palette() const override;

    // Window tabbing
    virtual QString caption(int idx) const override;
    virtual void closeTab(long id) override;
    virtual void closeTabGroup() override;
    virtual long currentTabId() const override;
    virtual QIcon icon(int idx) const override;
    virtual void setCurrentTab(long id) override;
    virtual void showWindowMenu(const QPoint &, long id) override;
    virtual void tab_A_before_B(long A, long B) override;
    virtual void tab_A_behind_B(long A, long B) override;
    virtual int tabCount() const override;
    virtual long tabId(int idx) const override;
    virtual void untab(long id, const QRect& newGeom) override;
    virtual WindowOperation buttonToWindowOperation(Qt::MouseButtons button) override;

private:
    KDecorationPreview* preview;
    PreviewItem *m_previewItem;
    bool active;
};

class KDecorationPreviewOptions
    : public KDecorationOptions
{
public:
    KDecorationPreviewOptions();
    virtual ~KDecorationPreviewOptions();
    void updateSettings();

    void setCustomBorderSize(BorderSize size);
    void setCustomTitleButtonsEnabled(bool enabled);
    void setCustomTitleButtons(const QList<DecorationButton> &left, const QList<DecorationButton> &right);

private:
    BorderSize customBorderSize;
    bool customButtonsChanged;
    bool customButtons;
    QList<DecorationButton> customTitleButtonsLeft;
    QList<DecorationButton> customTitleButtonsRight;
};

class KDecorationPreviewPlugins
    : public KDecorationPlugins
{
public:
    explicit KDecorationPreviewPlugins(const KSharedConfigPtr &cfg);
    virtual bool provides(Requirement);
};

inline KDecorationPreviewPlugins::KDecorationPreviewPlugins(const KSharedConfigPtr &cfg)
    : KDecorationPlugins(cfg)
{
}

#endif
