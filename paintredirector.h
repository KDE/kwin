/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2009 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#ifndef PAINTREDIRECTOR_H
#define PAINTREDIRECTOR_H

#include <qregion.h>
#include <qtimer.h>
#include <qwidget.h>
#include <qbasictimer.h>

namespace KWin
{
// forward declarations
class Client;
class Deleted;

// This class redirects all painting of a given widget (including its children)
// into a paint device (QPixmap).
class PaintRedirector
    : public QObject
{
    Q_OBJECT
public:
    enum DecorationPixmap {
        TopPixmap,
        RightPixmap,
        BottomPixmap,
        LeftPixmap,
        PixmapCount
    };
    PaintRedirector(Client *c, QWidget* widget);
    virtual ~PaintRedirector();
    QPixmap performPendingPaint();
    virtual bool eventFilter(QObject* o, QEvent* e);
    QRegion pendingRegion() const;
    QRegion scheduledRepaintRegion();

    void markAsRepainted() {
        m_requiresRepaint = false;
    }
    bool requiresRepaint() const {
        return m_requiresRepaint;
    }
    void resizePixmaps();

    const QPixmap *topDecoPixmap() const {
        return &m_pixmaps[TopPixmap];
    }
    const QPixmap *leftDecoPixmap() const {
        return &m_pixmaps[LeftPixmap];
    }
    const QPixmap *bottomDecoPixmap() const {
        return &m_pixmaps[BottomPixmap];
    }
    const QPixmap *rightDecoPixmap() const {
        return &m_pixmaps[RightPixmap];
    }

    /**
     * Used by Deleted::copyToDeleted() to move the PaintRedirector to the Deleted.
     * The actual redirecting ends as the decoration gets destroyed after the Deleted
     * is created.
     **/
    void reparent(Deleted *d);

public slots:
    void ensurePixmapsPainted();
private:
    void added(QWidget* widget);
    void removed(QWidget* widget);
    bool isToolTip(QWidget* widget) const;
    void timerEvent(QTimerEvent* event);

    void repaintPixmap(QPixmap& pix, const QRect& r, const QPixmap& src, QRegion reg);
    QWidget* widget;
    QRegion pending;
    QRegion scheduled;
    QPixmap scratch;
    bool recursionCheck;
    QBasicTimer cleanupTimer;

    Client *m_client;
    // we (instead of Qt) initialize the Pixmaps, and have to free them
    bool m_responsibleForPixmap;
    bool m_requiresRepaint;
    QPixmap m_pixmaps[PixmapCount];
};

} // namespace

#endif
