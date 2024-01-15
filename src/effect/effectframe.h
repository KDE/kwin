/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"
#include "effect/offscreenquickview.h"

#include <QFont>
#include <QIcon>

namespace KWin
{

class RenderTarget;
class RenderViewport;

/**
 * Style types used by @ref EffectFrame.
 * @since 4.6
 */
enum EffectFrameStyle {
    EffectFrameNone, ///< Displays no frame around the contents.
    EffectFrameUnstyled, ///< Displays a basic box around the contents.
    EffectFrameStyled ///< Displays a Plasma-styled frame around the contents.
};

class EffectFrameQuickScene : public OffscreenQuickScene
{
    Q_OBJECT

    Q_PROPERTY(QFont font READ font NOTIFY fontChanged)
    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(QSize iconSize READ iconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(qreal frameOpacity READ frameOpacity NOTIFY frameOpacityChanged)
    Q_PROPERTY(bool crossFadeEnabled READ crossFadeEnabled NOTIFY crossFadeEnabledChanged)
    Q_PROPERTY(qreal crossFadeProgress READ crossFadeProgress NOTIFY crossFadeProgressChanged)

public:
    EffectFrameQuickScene(EffectFrameStyle style, bool staticSize, QPoint position, Qt::Alignment alignment);
    ~EffectFrameQuickScene() override;

    EffectFrameStyle style() const;
    bool isStatic() const;

    QFont font() const;
    void setFont(const QFont &font);
    Q_SIGNAL void fontChanged(const QFont &font);

    QIcon icon() const;
    void setIcon(const QIcon &icon);
    Q_SIGNAL void iconChanged(const QIcon &icon);

    QSize iconSize() const;
    void setIconSize(const QSize &iconSize);
    Q_SIGNAL void iconSizeChanged(const QSize &iconSize);

    QString text() const;
    void setText(const QString &text);
    Q_SIGNAL void textChanged(const QString &text);

    qreal frameOpacity() const;
    void setFrameOpacity(qreal frameOpacity);
    Q_SIGNAL void frameOpacityChanged(qreal frameOpacity);

    bool crossFadeEnabled() const;
    void setCrossFadeEnabled(bool enabled);
    Q_SIGNAL void crossFadeEnabledChanged(bool enabled);

    qreal crossFadeProgress() const;
    void setCrossFadeProgress(qreal progress);
    Q_SIGNAL void crossFadeProgressChanged(qreal progress);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    QPoint position() const;
    void setPosition(const QPoint &point);

private:
    void reposition();

    EffectFrameStyle m_style;

    // Position
    bool m_static;
    QPoint m_point;
    Qt::Alignment m_alignment;

    // Contents
    QFont m_font;
    QIcon m_icon;
    QSize m_iconSize;
    QString m_text;
    qreal m_frameOpacity = 0.0;
    bool m_crossFadeEnabled = false;
    qreal m_crossFadeProgress = 0.0;
};

/**
 * @short Helper class for displaying text and icons in frames.
 *
 * Paints text and/or and icon with an optional frame around them. The
 * available frames includes one that follows the default Plasma theme and
 * another that doesn't.
 * It is recommended to use this class whenever displaying text.
 */
class KWIN_EXPORT EffectFrame : public QObject
{
    Q_OBJECT

public:
    explicit EffectFrame(EffectFrameStyle style, bool staticSize = true, QPoint position = QPoint(-1, -1),
                         Qt::Alignment alignment = Qt::AlignCenter);
    ~EffectFrame();

    /**
     * Delete any existing textures to free up graphics memory. They will
     * be automatically recreated the next time they are required.
     */
    void free();

    /**
     * Render the frame.
     */
    void render(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region = infiniteRegion(), double opacity = 1.0, double frameOpacity = 1.0);

    void setPosition(const QPoint &point);
    /**
     * Set the text alignment for static frames and the position alignment
     * for non-static.
     */
    void setAlignment(Qt::Alignment alignment);
    Qt::Alignment alignment() const;
    void setGeometry(const QRect &geometry, bool force = false);
    QRect geometry() const;

    void setText(const QString &text);
    QString text() const;
    void setFont(const QFont &font);
    QFont font() const;
    /**
     * Set the icon that will appear on the left-hand size of the frame.
     */
    void setIcon(const QIcon &icon);
    QIcon icon() const;
    void setIconSize(const QSize &size);
    QSize iconSize() const;

    /**
     * @returns The style of this EffectFrame.
     */
    EffectFrameStyle style() const;

    /**
     * If @p enable is @c true cross fading between icons and text is enabled
     * By default disabled. Use setCrossFadeProgress to cross fade.
     * Cross Fading is currently only available if OpenGL is used.
     * @param enable @c true enables cross fading, @c false disables it again
     * @see isCrossFade
     * @see setCrossFadeProgress
     * @since 4.6
     */
    void enableCrossFade(bool enable);
    /**
     * @returns @c true if cross fading is enabled, @c false otherwise
     * @see enableCrossFade
     * @since 4.6
     */
    bool isCrossFade() const;
    /**
     * Sets the current progress for cross fading the last used icon/text
     * with current icon/text to @p progress.
     * A value of 0.0 means completely old icon/text, a value of 1.0 means
     * completely current icon/text.
     * Default value is 1.0. You have to enable cross fade before using it.
     * Cross Fading is currently only available if OpenGL is used.
     * @see enableCrossFade
     * @see isCrossFade
     * @see crossFadeProgress
     * @since 4.6
     */
    void setCrossFadeProgress(qreal progress);
    /**
     * @returns The current progress for cross fading
     * @see setCrossFadeProgress
     * @see enableCrossFade
     * @see isCrossFade
     * @since 4.6
     */
    qreal crossFadeProgress() const;

private:
    EffectFrameQuickScene *m_view;
};

} // namespace KWin
