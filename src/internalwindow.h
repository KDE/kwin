/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/graphicsbuffer.h"
#include "window.h"

namespace KWin
{

struct InternalWindowFrame
{
    GraphicsBuffer *buffer = nullptr;
    QRegion bufferDamage;
    OutputTransform bufferTransform = OutputTransform::Normal;
};

class KWIN_EXPORT InternalWindow : public Window
{
    Q_OBJECT

public:
    explicit InternalWindow(QWindow *handle);
    ~InternalWindow() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    QString captionNormal() const override;
    QString captionSuffix() const override;
    QSizeF minSize() const override;
    QSizeF maxSize() const override;
    WindowType windowType() const override;
    void killWindow() override;
    bool isClient() const override;
    bool isPopupWindow() const override;
    QString windowRole() const override;
    void closeWindow() override;
    bool isCloseable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;
    bool isPlaceable() const override;
    bool noBorder() const override;
    bool userCanSetNoBorder() const override;
    bool wantsInput() const override;
    bool isInternal() const override;
    bool isLockScreen() const override;
    bool isOutline() const override;
    QRectF resizeWithChecks(const QRectF &geometry, const QSizeF &size) override;
    bool takeFocus() override;
    void setNoBorder(bool set) override;
    void invalidateDecoration() override;
    void destroyWindow() override;
    bool hasPopupGrab() const override;
    void popupDone() override;
    bool hitTest(const QPointF &point) const override;
    void pointerEnterEvent(const QPointF &globalPos) override;
    void pointerLeaveEvent() override;

    GraphicsBuffer *graphicsBuffer() const;
    OutputTransform bufferTransform() const;

    void present(const InternalWindowFrame &frame);
    qreal bufferScale() const;
    QWindow *handle() const;

Q_SIGNALS:
    void presented(const InternalWindowFrame &frame);

protected:
    bool acceptsFocus() const override;
    bool belongsToSameApplication(const Window *other, SameApplicationChecks checks) const override;
    void doInteractiveResizeSync(const QRectF &rect) override;
    void updateCaption() override;
    void moveResizeInternal(const QRectF &rect, MoveResizeMode mode) override;
    std::unique_ptr<WindowItem> createItem(Item *parentItem) override;

private:
    void requestGeometry(const QRectF &rect);
    void commitGeometry(const QRectF &rect);
    void setCaption(const QString &caption);
    void markAsMapped();
    void syncGeometryToInternalWindow();
    void updateInternalWindowGeometry();
    void updateDecoration(bool check_workspace_pos, bool force = false);
    void createDecoration(const QRectF &oldGeometry);
    void destroyDecoration();

    QWindow *m_handle = nullptr;
    QString m_captionNormal;
    QString m_captionSuffix;
    Qt::WindowFlags m_internalWindowFlags = Qt::WindowFlags();
    bool m_userNoBorder = false;
    GraphicsBufferRef m_graphicsBufferRef;
    OutputTransform m_bufferTransform = OutputTransform::Normal;

    Q_DISABLE_COPY(InternalWindow)
};

}
