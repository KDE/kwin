/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "main.h"

namespace KWin
{

class KWinSelectionOwner;

class ApplicationX11 : public Application
{
    Q_OBJECT
public:
    ApplicationX11(int &argc, char **argv);
    ~ApplicationX11() override;

    void setReplace(bool replace);

    std::unique_ptr<Edge> createScreenEdge(ScreenEdges *parent) override;
    std::unique_ptr<Cursor> createPlatformCursor() override;
    std::unique_ptr<OutlineVisual> createOutline(Outline *outline) override;
    void createEffectsHandler(Compositor *compositor, WorkspaceScene *scene) override;
    void startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName = QByteArray()) override;
    void startInteractivePositionSelection(std::function<void(const QPointF &)> callback) override;
    PlatformCursorImage cursorImage() const override;

protected:
    void performStartup() override;

private Q_SLOTS:
    void lostSelection();

private:
    void crashChecking();
    void setupCrashHandler();
    void notifyKSplash();

    static void crashHandler(int signal);

    std::unique_ptr<KWinSelectionOwner> owner;
    bool m_replace;
};

}
