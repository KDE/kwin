/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QCoreApplication>
#include <QLoggingCategory>

#include <QtGeniviExtras/QDltRegistration>
#include <QtGeniviExtras/QtDlt>

namespace KWin
{

QDLT_REGISTER_CONTEXT_ON_FIRST_USE(true)
QDLT_REGISTER_APPLICATION("KWIN", "KWin Compositor")

Q_LOGGING_CATEGORY(KWIN_DLT_LOG, "kwin_log")
QDLT_REGISTER_LOGGING_CATEGORY(KWIN_DLT_LOG, "kwin", "LOG", "Other KWin logging")
QDLT_FALLBACK_CATEGORY(KWIN_DLT_LOG)

QLoggingCategory::CategoryFilter qtCategoryFilter = nullptr;
QtMessageHandler qtDefaultMessageHandler = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QDltRegistration::messageHandler(type, context, message);
    qtDefaultMessageHandler(type, context, message);
}

struct DltLoggingCategory
{
    const char *dltCtxId;
    const char *dltCtxDescription;
};

static const auto dltCategories = QHash<const char *, DltLoggingCategory>{
    {"kwin_core", {"CORE", "KWin Core"}},
    {"kwin_libinput", {"LINP", "KWin libinput backend"}},
    {"kwin_scene_opengl", {"SOGL", "KWin OpenGL scene"}},
    {"kwin_scene_qpainter", {"SQPR", "KWin QPainter scene"}},
    {"kwin_scripting", {"SCRP", "KWin scripting"}},
    {"kwin_wayland_drm", {"WDRM", "KWin Wayland DRM backend"}},
    {"kwin_virtualkeyboard", {"VKB", "KWin virtual keyboard"}},
    {"kwin_xkbcommon", {"XKB", "KWin xkbcommon"}},

    {"libkwineffects", {"LKFX", "libkwineffects"}},
    {"libkwinglutils", {"LKGU", "libkwinglutils"}},
};

void setupDltLogging()
{
    qtCategoryFilter = QLoggingCategory::installFilter([](QLoggingCategory *category) {
        auto it = dltCategories.find(category->categoryName());
        if (it != dltCategories.end()) {
            globalDltRegistration()->registerCategory(category, it->dltCtxId, it->dltCtxDescription);
        }

        if (qtCategoryFilter) {
            qtCategoryFilter(category);
        }
    });

    qtDefaultMessageHandler = qInstallMessageHandler(messageHandler);
}

Q_COREAPP_STARTUP_FUNCTION(setupDltLogging)

} // namespace KWin
