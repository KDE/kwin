/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
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

#include <QCommandLineParser>
#include <QApplication>
#include <kconfig.h>
#include <KLocalizedString>
#include <kwindowsystem.h>

#include "ruleswidget.h"
#include "../../rules.h"
#include <QByteArray>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUuid>

Q_DECLARE_METATYPE(NET::WindowType)

namespace KWin
{

static void loadRules(QList< Rules* >& rules)
{
    KConfig _cfg("kwinrulesrc", KConfig::NoGlobals);
    KConfigGroup cfg(&_cfg, "General");
    int count = cfg.readEntry("count", 0);
    for (int i = 1;
            i <= count;
            ++i) {
        cfg = KConfigGroup(&_cfg, QString::number(i));
        Rules* rule = new Rules(cfg);
        rules.append(rule);
    }
}

static void saveRules(const QList< Rules* >& rules)
{
    KConfig cfg("kwinrulesrc", KConfig::NoGlobals);
    QStringList groups = cfg.groupList();
    for (QStringList::ConstIterator it = groups.constBegin();
            it != groups.constEnd();
            ++it)
        cfg.deleteGroup(*it);
    cfg.group("General").writeEntry("count", rules.count());
    int i = 1;
    for (QList< Rules* >::ConstIterator it = rules.constBegin();
            it != rules.constEnd();
            ++it) {
        KConfigGroup cg(&cfg, QString::number(i));
        (*it)->write(cg);
        ++i;
    }
}

static Rules* findRule(const QList< Rules* >& rules, const QVariantMap &data, bool whole_app)
{
    QByteArray wmclass_class = data.value("resourceClass").toByteArray().toLower();
    QByteArray wmclass_name = data.value("resourceName").toByteArray().toLower();
    QByteArray role = data.value("role").toByteArray().toLower();
    NET::WindowType type = data.value("type").value<NET::WindowType>();
    QString title = data.value("caption").toString();
    QByteArray machine = data.value("clientMachine").toByteArray();
    Rules* best_match = nullptr;
    int match_quality = 0;
    for (QList< Rules* >::ConstIterator it = rules.constBegin();
            it != rules.constEnd();
            ++it) {
        // try to find an exact match, i.e. not a generic rule
        Rules* rule = *it;
        int quality = 0;
        bool generic = true;
        if (rule->wmclassmatch != Rules::ExactMatch)
            continue; // too generic
        if (!rule->matchWMClass(wmclass_class, wmclass_name))
            continue;
        // from now on, it matches the app - now try to match for a specific window
        if (rule->wmclasscomplete) {
            quality += 1;
            generic = false;  // this can be considered specific enough (old X apps)
        }
        if (!whole_app) {
            if (rule->windowrolematch != Rules::UnimportantMatch) {
                quality += rule->windowrolematch == Rules::ExactMatch ? 5 : 1;
                generic = false;
            }
            if (rule->titlematch != Rules::UnimportantMatch) {
                quality += rule->titlematch == Rules::ExactMatch ? 3 : 1;
                generic = false;
            }
            if (rule->types != NET::AllTypesMask) {
                int bits = 0;
                for (unsigned int bit = 1;
                        bit < 1U << 31;
                        bit <<= 1)
                    if (rule->types & bit)
                        ++bits;
                if (bits == 1)
                    quality += 2;
            }
            if (generic)   // ignore generic rules, use only the ones that are for this window
                continue;
        } else {
            if (rule->types == NET::AllTypesMask)
                quality += 2;
        }
        if (!rule->matchType(type)
                || !rule->matchRole(role)
                || !rule->matchTitle(title)
                || !rule->matchClientMachine(machine, data.value("localhost").toBool()))
            continue;
        if (quality > match_quality) {
            best_match = rule;
            match_quality = quality;
        }
    }
    if (best_match != nullptr)
        return best_match;
    Rules* ret = new Rules;
    if (whole_app) {
        ret->description = i18n("Application settings for %1", QString::fromLatin1(wmclass_class));
        // TODO maybe exclude some types? If yes, then also exclude them above
        // when searching.
        ret->types = NET::AllTypesMask;
        ret->titlematch = Rules::UnimportantMatch;
        ret->clientmachine = machine; // set, but make unimportant
        ret->clientmachinematch = Rules::UnimportantMatch;
        ret->windowrolematch = Rules::UnimportantMatch;
        if (wmclass_name == wmclass_class) {
            ret->wmclasscomplete = false;
            ret->wmclass = wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
        } else {
            // WM_CLASS components differ - perhaps the app got -name argument
            ret->wmclasscomplete = true;
            ret->wmclass = wmclass_name + ' ' + wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
        }
        return ret;
    }
    ret->description = i18n("Window settings for %1", QString::fromLatin1(wmclass_class));
    if (type == NET::Unknown)
        ret->types = NET::NormalMask;
    else
        ret->types = NET::WindowTypeMask( 1 << type); // convert type to its mask
    ret->title = title; // set, but make unimportant
    ret->titlematch = Rules::UnimportantMatch;
    ret->clientmachine = machine; // set, but make unimportant
    ret->clientmachinematch = Rules::UnimportantMatch;
    if (!role.isEmpty()
            && role != "unknown" && role != "unnamed") { // Qt sets this if not specified
        ret->windowrole = role;
        ret->windowrolematch = Rules::ExactMatch;
        if (wmclass_name == wmclass_class) {
            ret->wmclasscomplete = false;
            ret->wmclass = wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
        } else {
            // WM_CLASS components differ - perhaps the app got -name argument
            ret->wmclasscomplete = true;
            ret->wmclass = wmclass_name + ' ' + wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
        }
    } else { // no role set
        if (wmclass_name != wmclass_class) {
            ret->wmclasscomplete = true;
            ret->wmclass = wmclass_name + ' ' + wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
        } else {
            // This is a window that has no role set, and both components of WM_CLASS
            // match (possibly only differing in case), which most likely means either
            // the application doesn't give a damn about distinguishing its various
            // windows, or it's an app that uses role for that, but this window
            // lacks it for some reason. Use non-complete WM_CLASS matching, also
            // include window title in the matching, and pray it causes many more positive
            // matches than negative matches.
            ret->titlematch = Rules::ExactMatch;
            ret->wmclasscomplete = false;
            ret->wmclass = wmclass_class;
            ret->wmclassmatch = Rules::ExactMatch;
        }
    }
    return ret;
}

static void edit(const QVariantMap &data, bool whole_app)
{
    QList< Rules* > rules;
    loadRules(rules);
    Rules* orig_rule = findRule(rules, data, whole_app);
    RulesDialog dlg;
    if (whole_app)
        dlg.setWindowTitle(i18nc("Window caption for the application wide rules dialog", "Edit Application-Specific Settings"));
    // dlg.edit() creates new Rules instance if edited
    Rules* edited_rule = dlg.edit(orig_rule, data, true);
    if (edited_rule == nullptr || edited_rule->isEmpty()) {
        rules.removeAll(orig_rule);
        delete orig_rule;
        if (orig_rule != edited_rule)
            delete edited_rule;
    } else if (edited_rule != orig_rule) {
        int pos = rules.indexOf(orig_rule);
        if (pos != -1)
            rules[ pos ] = edited_rule;
        else
            rules.prepend(edited_rule);
        delete orig_rule;
    }
    saveRules(rules);
    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
    qApp->quit();
}

} // namespace

extern "C"
KWIN_EXPORT int kdemain(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationDisplayName(i18n("KWin"));
    app.setApplicationName("kwin_rules_dialog");
    app.setApplicationVersion("1.0");
    bool whole_app = false;
    QUuid uuid;
    {
        QCommandLineParser parser;
        parser.setApplicationDescription(i18n("KWin helper utility"));
        parser.addOption(QCommandLineOption("uuid", i18n("KWin id of the window for special window settings."), "uuid"));
        parser.addOption(QCommandLineOption("whole-app", i18n("Whether the settings should affect all windows of the application.")));
        parser.process(app);

        uuid = QUuid::fromString(parser.value("uuid"));
        whole_app = parser.isSet("whole-app");
    }


    if (uuid.isNull()) {
        printf("%s\n", qPrintable(i18n("This helper utility is not supposed to be called directly.")));
        return 1;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/KWin"),
                                                          QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("getWindowInfo"));
    message.setArguments({uuid.toString()});
    QDBusPendingReply<QVariantMap> async = QDBusConnection::sessionBus().asyncCall(message);

    QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(async, &app);
    QObject::connect(callWatcher, &QDBusPendingCallWatcher::finished, &app,
        [&whole_app] (QDBusPendingCallWatcher *self) {
            QDBusPendingReply<QVariantMap> reply = *self;
            self->deleteLater();
            if (!reply.isValid() || reply.value().isEmpty()) {
                qApp->quit();
                return;
            }
            KWin::edit(reply.value(), whole_app);
        }
    );



    return app.exec();
}
