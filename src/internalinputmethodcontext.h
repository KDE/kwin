/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QObject>
#include <QRect>
#include <qpa/qplatforminputcontext.h>

namespace KWin
{

class InternalInputMethodContext : public QPlatformInputContext
{
    Q_OBJECT
public:
    InternalInputMethodContext(QObject *parent);
    ~InternalInputMethodContext() override;

    // QPA -> kwin
    bool isEnabled() const;
    QString surroundingText() const;
    int cursorPosition() const;
    int anchorPosition() const;

    // QPA internal handlers
    bool isValid() const override;
    void update(Qt::InputMethodQueries) override;
    void showInputPanel() override;
    void hideInputPanel() override;
    bool isInputPanelVisible() const override;
    QRectF keyboardRect() const override;
    QLocale locale() const override;
    Qt::LayoutDirection inputDirection() const override;
    void setFocusObject(QObject *object) override;

    // from kwin to -> QPA
    void handlePreeditText(const QString &text, int cursorBegin, int cursorEnd);
    void handleCommitString(const QString &text);
    void handleDeleteSurroundingText(int index, uint length);

Q_SIGNALS:
    void cursorRectangleChanged(const QRect &rect);
    void contentTypeChanged();
    void surroundingTextChanged();
    void enabledChanged();
    void stateCommitted(quint32 serial);
    void enableRequested();
    void showInputPanelRequested();
    void hideInputPanelRequested();

private:
    int indexFromWayland(const QString &text, int lenght, int base);
    QString m_surroundingText;
    int m_cursor;
    int m_cursorPos;
    int m_anchorPos;
    uint32_t m_contentHint = 0;
    uint32_t m_contentPurpose = 0;
    QRect m_cursorRect;
};

}
