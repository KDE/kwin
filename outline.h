/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_OUTLINE_H
#define KWIN_OUTLINE_H
#include <kwinglobals.h>
#include <QRect>
#include <QObject>

#include <kwin_export.h>

class QQmlContext;
class QQmlComponent;

namespace KWin {
class OutlineVisual;

/**
 * @short This class is used to show the outline of a given geometry.
 *
 * The class renders an outline by using four windows. One for each border of
 * the geometry. It is possible to replace the outline with an effect. If an
 * effect is available the effect will be used, otherwise the outline will be
 * rendered by using the X implementation.
 *
 * @author Arthur Arlt
 * @since 4.7
 */
class Outline : public QObject {
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(QRect visualParentGeometry READ visualParentGeometry NOTIFY visualParentGeometryChanged)
    Q_PROPERTY(QRect unifiedGeometry READ unifiedGeometry NOTIFY unifiedGeometryChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    ~Outline() override;

    /**
     * Set the outline geometry.
     * To show the outline use showOutline.
     * @param outlineGeometry The geometry of the outline to be shown
     * @see showOutline
     */
    void setGeometry(const QRect &outlineGeometry);

    /**
     * Set the visual parent geometry.
     * This is the geometry from which the will emerge.
     * @param visualParentGeometry The visual geometry of the visual parent
     * @see showOutline
     */
    void setVisualParentGeometry(const QRect &visualParentGeometry);

    /**
     * Shows the outline of a window using either an effect or the X implementation.
     * To stop the outline process use hideOutline.
     * @see hideOutline
     */
    void show();

    /**
     * Shows the outline for the given @p outlineGeometry.
     * This is the same as setOutlineGeometry followed by showOutline directly.
     * To stop the outline process use hideOutline.
     * @param outlineGeometry The geometry of the outline to be shown
     * @see hideOutline
     */
    void show(const QRect &outlineGeometry);

    /**
     * Shows the outline for the given @p outlineGeometry animated from @p visualParentGeometry.
     * This is the same as setOutlineGeometry followed by setVisualParentGeometry
     * and then showOutline.
     * To stop the outline process use hideOutline.
     * @param outlineGeometry The geometry of the outline to be shown
     * @param visualParentGeometry The geometry from where the outline should emerge
     * @see hideOutline
     * @since 5.10
     */
    void show(const QRect &outlineGeometry, const QRect &visualParentGeometry);

    /**
     * Hides shown outline.
     * @see showOutline
     */
    void hide();

    const QRect &geometry() const;
    const QRect &visualParentGeometry() const;
    QRect unifiedGeometry() const;

    bool isActive() const;

private Q_SLOTS:
    void compositingChanged();

Q_SIGNALS:
    void activeChanged();
    void geometryChanged();
    void unifiedGeometryChanged();
    void visualParentGeometryChanged();

private:
    void createHelper();
    QScopedPointer<OutlineVisual> m_visual;
    QRect m_outlineGeometry;
    QRect m_visualParentGeometry;
    bool m_active;
    KWIN_SINGLETON(Outline)
};

class KWIN_EXPORT OutlineVisual
{
public:
    OutlineVisual(Outline *outline);
    virtual ~OutlineVisual();
    virtual void show() = 0;
    virtual void hide() = 0;
protected:
    Outline *outline();
    const Outline *outline() const;
private:
    Outline *m_outline;
};

class CompositedOutlineVisual : public OutlineVisual
{
public:
    CompositedOutlineVisual(Outline *outline);
    ~CompositedOutlineVisual() override;
    void show() override;
    void hide() override;
private:
    QScopedPointer<QQmlContext> m_qmlContext;
    QScopedPointer<QQmlComponent> m_qmlComponent;
    QScopedPointer<QObject> m_mainItem;
};

inline
bool Outline::isActive() const
{
    return m_active;
}

inline
const QRect &Outline::geometry() const
{
    return m_outlineGeometry;
}

inline
const QRect &Outline::visualParentGeometry() const
{
    return m_visualParentGeometry;
}

inline
Outline *OutlineVisual::outline()
{
    return m_outline;
}

inline
const Outline *OutlineVisual::outline() const
{
    return m_outline;
}

inline
Outline *outline()
{
    return Outline::self();
}

}

#endif
