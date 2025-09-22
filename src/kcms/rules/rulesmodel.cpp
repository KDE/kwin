/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "rulesmodel.h"

#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QIcon>
#include <QQmlEngine>
#include <QTimer>

#include <KColorSchemeManager>
#include <KConfig>
#include <KLocalizedString>
#include <KWindowSystem>

namespace KWin
{

RulesModel::RulesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qmlRegisterUncreatableType<RuleItem>("org.kde.kcms.kwinrules", 1, 0, "RuleItem",
                                         QStringLiteral("Do not create objects of type RuleItem"));
    qmlRegisterUncreatableType<RulesModel>("org.kde.kcms.kwinrules", 1, 0, "RulesModel",
                                           QStringLiteral("Do not create objects of type RulesModel"));
    qmlRegisterUncreatableType<OptionsModel>("org.kde.kcms.kwinrules", 1, 0, "OptionsModel",
                                             QStringLiteral("Do not create objects of type OptionsModel"));

    qDBusRegisterMetaType<KWin::DBusDesktopDataStruct>();
    qDBusRegisterMetaType<KWin::DBusDesktopDataVector>();

    populateRuleList();
}

RulesModel::~RulesModel()
{
}

QHash<int, QByteArray> RulesModel::roleNames() const
{
    return {
        {KeyRole, QByteArrayLiteral("key")},
        {NameRole, QByteArrayLiteral("name")},
        {IconRole, QByteArrayLiteral("icon")},
        {IconNameRole, QByteArrayLiteral("iconName")},
        {SectionRole, QByteArrayLiteral("section")},
        {DescriptionRole, QByteArrayLiteral("description")},
        {EnabledRole, QByteArrayLiteral("enabled")},
        {SelectableRole, QByteArrayLiteral("selectable")},
        {ValueRole, QByteArrayLiteral("value")},
        {TypeRole, QByteArrayLiteral("type")},
        {PolicyRole, QByteArrayLiteral("policy")},
        {PolicyModelRole, QByteArrayLiteral("policyModel")},
        {OptionsModelRole, QByteArrayLiteral("options")},
        {SuggestedValueRole, QByteArrayLiteral("suggested")},
    };
}

int RulesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_ruleList.size();
}

QVariant RulesModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }

    const RuleItem *rule = m_ruleList.at(index.row());

    switch (role) {
    case KeyRole:
        return rule->key();
    case NameRole:
        return rule->name();
    case IconRole:
        return rule->icon();
    case IconNameRole:
        return rule->iconName();
    case DescriptionRole:
        return rule->description();
    case SectionRole:
        return rule->section();
    case EnabledRole:
        return rule->isEnabled();
    case SelectableRole:
        return !rule->hasFlag(RuleItem::AlwaysEnabled) && !rule->hasFlag(RuleItem::SuggestionOnly);
    case ValueRole:
        return rule->value();
    case TypeRole:
        return rule->type();
    case PolicyRole:
        return rule->policy();
    case PolicyModelRole:
        return rule->policyModel();
    case OptionsModelRole:
        return rule->options();
    case SuggestedValueRole:
        return rule->suggestedValue();
    }
    return QVariant();
}

bool RulesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return false;
    }

    RuleItem *rule = m_ruleList.at(index.row());

    switch (role) {
    case EnabledRole:
        if (value.toBool() == rule->isEnabled()) {
            return true;
        }
        rule->setEnabled(value.toBool());
        break;
    case ValueRole:
        if (rule->hasFlag(RuleItem::SuggestionOnly)) {
            processSuggestion(rule->key(), value);
        }
        if (value == rule->value()) {
            return true;
        }
        rule->setValue(value);
        break;
    case PolicyRole:
        if (value.toInt() == rule->policy()) {
            return true;
        }
        rule->setPolicy(value.toInt());
        break;
    case SuggestedValueRole:
        if (value == rule->suggestedValue()) {
            return true;
        }
        rule->setSuggestedValue(value);
        break;
    default:
        return false;
    }

    writeToSettings(rule);

    Q_EMIT dataChanged(index, index, QList<int>{role});
    if (rule->hasFlag(RuleItem::AffectsDescription)) {
        Q_EMIT descriptionChanged();
    }
    if (rule->hasFlag(RuleItem::AffectsWarning)) {
        Q_EMIT warningMessagesChanged();
    }

    return true;
}

QModelIndex RulesModel::indexOf(const QString &key) const
{
    const QModelIndexList indexes = match(index(0), RulesModel::KeyRole, key, 1, Qt::MatchFixedString);
    if (indexes.isEmpty()) {
        return QModelIndex();
    }
    return indexes.at(0);
}

RuleItem *RulesModel::addRule(RuleItem *rule)
{
    m_ruleList << rule;
    m_rules.insert(rule->key(), rule);

    return rule;
}

bool RulesModel::hasRule(const QString &key) const
{
    return m_rules.contains(key);
}

RuleItem *RulesModel::ruleItem(const QString &key) const
{
    return m_rules.value(key);
}

QString RulesModel::description() const
{
    const QString desc = m_rules["description"]->value().toString();
    if (!desc.isEmpty()) {
        return desc;
    }
    return defaultDescription();
}

void RulesModel::setDescription(const QString &description)
{
    setData(indexOf("description"), description, RulesModel::ValueRole);
}

QString RulesModel::defaultDescription() const
{
    const QString wmclass = m_rules["wmclass"]->value().toString();
    const QString title = m_rules["title"]->isEnabled() ? m_rules["title"]->value().toString() : QString();

    if (!title.isEmpty()) {
        return i18n("Window settings for %1", title);
    }
    if (m_rules["tag"]->isEnabled()) {
        return i18n("Window settings for %1", m_rules["tag"]->value().toString());
    }
    if (!wmclass.isEmpty()) {
        return i18n("Settings for %1", wmclass);
    }

    return i18n("New window settings");
}

void RulesModel::processSuggestion(const QString &key, const QVariant &value)
{
    if (key == QLatin1String("wmclasshelper")) {
        setData(indexOf("wmclass"), value, RulesModel::ValueRole);
        setData(indexOf("wmclasscomplete"), true, RulesModel::ValueRole);
    }
}

QStringList RulesModel::warningMessages() const
{
    QStringList messages;

    if (wmclassWarning()) {
        messages << i18n("You have specified the window class as unimportant.\n"
                         "This means the settings will possibly apply to windows from all applications."
                         " If you really want to create a generic setting, it is recommended"
                         " you at least limit the window types to avoid special window types.");
    }

    if (geometryWarning()) {
        messages << i18n("Some applications set their own geometry after starting,"
                         " overriding your initial settings for size and position. "
                         "To enforce these settings, also force the property \"%1\" to \"Yes\".",
                         m_rules["ignoregeometry"]->name());
    }

    if (opacityWarning()) {
        messages << i18n("Readability may be impaired with extremely low opacity values. At 0%, the window becomes invisible.");
    }

    return messages;
}

bool RulesModel::wmclassWarning() const
{
    const bool no_wmclass = !m_rules["wmclass"]->isEnabled()
        || m_rules["wmclass"]->policy() == Rules::UnimportantMatch;
    const bool alltypes = !m_rules["types"]->isEnabled()
        || (m_rules["types"]->value() == 0)
        || (m_rules["types"]->value() == NET::AllTypesMask)
        || ((m_rules["types"]->value().toInt() | (1 << NET::Override)) == 0x3FF);

    return (no_wmclass && alltypes);
}

bool RulesModel::geometryWarning() const
{
    const bool ignoregeometry = m_rules["ignoregeometry"]->isEnabled()
        && m_rules["ignoregeometry"]->policy() == Rules::Force
        && m_rules["ignoregeometry"]->value() == true;

    const bool initialPos = m_rules["position"]->isEnabled()
        && (m_rules["position"]->policy() == Rules::Apply
            || m_rules["position"]->policy() == Rules::Remember);

    const bool initialSize = m_rules["size"]->isEnabled()
        && (m_rules["size"]->policy() == Rules::Apply
            || m_rules["size"]->policy() == Rules::Remember);

    const bool initialPlacement = m_rules["placement"]->isEnabled()
        && m_rules["placement"]->policy() == Rules::Force;

    return (!ignoregeometry && (initialPos || initialSize || initialPlacement));
}

bool RulesModel::opacityWarning() const
{
    auto opacityActive = m_rules["opacityactive"];
    const bool lowOpacityActive = opacityActive->isEnabled()
        && opacityActive->policy() != Rules::Unused && opacityActive->policy() != Rules::DontAffect
        && opacityActive->value().toInt() < 25;

    auto opacityInactive = m_rules["opacityinactive"];
    const bool lowOpacityInactive = opacityInactive->isEnabled()
        && opacityActive->policy() != Rules::Unused && opacityActive->policy() != Rules::DontAffect
        && opacityInactive->value().toInt() < 25;

    return lowOpacityActive || lowOpacityInactive;
}

RuleSettings *RulesModel::settings() const
{
    return m_settings;
}

void RulesModel::setSettings(RuleSettings *settings)
{
    if (m_settings == settings) {
        return;
    }

    beginResetModel();

    m_settings = settings;

    for (RuleItem *rule : std::as_const(m_ruleList)) {
        const KConfigSkeletonItem *configItem = m_settings->findItem(rule->key());
        const KConfigSkeletonItem *configPolicyItem = m_settings->findItem(rule->policyKey());

        rule->reset();

        if (!configItem) {
            continue;
        }

        const bool isEnabled = configPolicyItem ? configPolicyItem->property() != Rules::Unused
                                                : !configItem->property().toString().isEmpty();
        rule->setEnabled(isEnabled);

        const QVariant value = configItem->property();
        rule->setValue(value);

        if (configPolicyItem) {
            const int policy = configPolicyItem->property().toInt();
            rule->setPolicy(policy);
        }
    }

    endResetModel();

    Q_EMIT descriptionChanged();
    Q_EMIT warningMessagesChanged();
}

void RulesModel::writeToSettings(RuleItem *rule)
{
    KConfigSkeletonItem *configItem = m_settings->findItem(rule->key());
    KConfigSkeletonItem *configPolicyItem = m_settings->findItem(rule->policyKey());

    if (!configItem) {
        return;
    }

    if (rule->isEnabled()) {
        configItem->setProperty(rule->value());
        if (configPolicyItem) {
            configPolicyItem->setProperty(rule->policy());
        }
    } else {
        configItem->setDefault();
        if (configPolicyItem) {
            configPolicyItem->setDefault();
        }
    }
}

void RulesModel::populateRuleList()
{
    qDeleteAll(m_ruleList);
    m_ruleList.clear();

    // Rule description
    auto description = addRule(new RuleItem(QLatin1String("description"),
                                            RulePolicy::NoPolicy, RuleItem::String,
                                            i18n("Description"), i18n("Window matching"),
                                            QIcon::fromTheme("entry-edit")));
    description->setFlag(RuleItem::AlwaysEnabled);
    description->setFlag(RuleItem::AffectsDescription);

    // Window matching
    auto wmclass = addRule(new RuleItem(QLatin1String("wmclass"),
                                        RulePolicy::StringMatch, RuleItem::String,
                                        i18n("Window class (application)"), i18n("Window matching"),
                                        QIcon::fromTheme("window")));
    wmclass->setFlag(RuleItem::AlwaysEnabled);
    wmclass->setFlag(RuleItem::AffectsDescription);
    wmclass->setFlag(RuleItem::AffectsWarning);

    auto wmclasscomplete = addRule(new RuleItem(QLatin1String("wmclasscomplete"),
                                                RulePolicy::NoPolicy, RuleItem::Boolean,
                                                i18n("Match whole window class"), i18n("Window matching"),
                                                QIcon::fromTheme("window")));
    wmclasscomplete->setFlag(RuleItem::AlwaysEnabled);

    // Helper item to store the detected whole window class when detecting properties
    auto wmclasshelper = addRule(new RuleItem(QLatin1String("wmclasshelper"),
                                              RulePolicy::NoPolicy, RuleItem::String,
                                              i18n("Whole window class"), i18n("Window matching"),
                                              QIcon::fromTheme("window")));
    wmclasshelper->setFlag(RuleItem::SuggestionOnly);

    auto types = addRule(new RuleItem(QLatin1String("types"),
                                      RulePolicy::NoPolicy, RuleItem::NetTypes,
                                      i18n("Window types"), i18n("Window matching"),
                                      QIcon::fromTheme("window-duplicate")));
    types->setOptionsData(windowTypesModelData());
    types->setFlag(RuleItem::AlwaysEnabled);
    types->setFlag(RuleItem::AffectsWarning);

    addRule(new RuleItem(QLatin1String("windowrole"),
                         RulePolicy::StringMatch, RuleItem::String,
                         i18n("Window role"), i18n("Window matching"),
                         QIcon::fromTheme("dialog-object-properties")));

    auto title = addRule(new RuleItem(QLatin1String("title"),
                                      RulePolicy::StringMatch, RuleItem::String,
                                      i18n("Window title"), i18n("Window matching"),
                                      QIcon::fromTheme("edit-comment")));
    title->setFlag(RuleItem::AffectsDescription);

    addRule(new RuleItem(QLatin1String("clientmachine"),
                         RulePolicy::StringMatch, RuleItem::String,
                         i18n("Machine (hostname)"), i18n("Window matching"),
                         QIcon::fromTheme("computer")));

    auto tag = addRule(new RuleItem(QLatin1String("tag"),
                                    RulePolicy::StringMatch, RuleItem::String,
                                    i18n("Window tag"), i18n("Window matching"),
                                    QIcon::fromTheme("edit-comment")));
    tag->setFlag(RuleItem::AffectsDescription);

    // Size & Position
    auto position = addRule(new RuleItem(QLatin1String("position"),
                                         RulePolicy::SetRule, RuleItem::Point,
                                         i18n("Position"), i18n("Size & Position"),
                                         QIcon::fromTheme("transform-move")));
    position->setFlag(RuleItem::AffectsWarning);

    auto size = addRule(new RuleItem(QLatin1String("size"),
                                     RulePolicy::SetRule, RuleItem::Size,
                                     i18n("Size"), i18n("Size & Position"),
                                     QIcon::fromTheme("transform-scale")));
    size->setFlag(RuleItem::AffectsWarning);

    addRule(new RuleItem(QLatin1String("maximizehoriz"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Maximized horizontally"), i18n("Size & Position"),
                         QIcon::fromTheme("resizecol")));

    addRule(new RuleItem(QLatin1String("maximizevert"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Maximized vertically"), i18n("Size & Position"),
                         QIcon::fromTheme("resizerow")));

    RuleItem *desktops;
    if (KWindowSystem::isPlatformX11()) {
        // Single selection of Virtual Desktop on X11
        desktops = new RuleItem(QLatin1String("desktops"),
                                RulePolicy::SetRule, RuleItem::Option,
                                i18n("Virtual desktop"), i18n("Size & Position"),
                                QIcon::fromTheme("virtual-desktops"));
    } else {
        // Multiple selection on Wayland
        desktops = new RuleItem(QLatin1String("desktops"),
                                RulePolicy::SetRule, RuleItem::OptionList,
                                i18n("Virtual desktops"), i18n("Size & Position"),
                                QIcon::fromTheme("virtual-desktops"));
    }
    addRule(desktops);
    desktops->setOptionsData(virtualDesktopsModelData());

    connect(this, &RulesModel::virtualDesktopsUpdated, this, [this]() {
        m_rules["desktops"]->setOptionsData(virtualDesktopsModelData());
        const QModelIndex index = indexOf("desktops");
        Q_EMIT dataChanged(index, index, {OptionsModelRole});
    });

    updateVirtualDesktops();

#if KWIN_BUILD_ACTIVITIES
    m_activities = new KActivities::Consumer(this);

    auto activity = addRule(new RuleItem(QLatin1String("activity"),
                                         RulePolicy::SetRule, RuleItem::OptionList,
                                         i18n("Activities"), i18n("Size & Position"),
                                         QIcon::fromTheme("activities")));
    activity->setOptionsData(activitiesModelData());

    // Activities consumer may update the available activities later
    auto updateActivities = [this]() {
        m_rules["activity"]->setOptionsData(activitiesModelData());
        const QModelIndex index = indexOf("activity");
        Q_EMIT dataChanged(index, index, {OptionsModelRole});
    };
    connect(m_activities, &KActivities::Consumer::activitiesChanged, this, updateActivities);
    connect(m_activities, &KActivities::Consumer::serviceStatusChanged, this, updateActivities);
#endif

    addRule(new RuleItem(QLatin1String("screen"),
                         RulePolicy::SetRule, RuleItem::Integer,
                         i18n("Screen"), i18n("Size & Position"),
                         QIcon::fromTheme("osd-shutd-screen")));

    addRule(new RuleItem(QLatin1String("fullscreen"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Fullscreen"), i18n("Size & Position"),
                         QIcon::fromTheme("view-fullscreen")));

    addRule(new RuleItem(QLatin1String("minimize"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Minimized"), i18n("Size & Position"),
                         QIcon::fromTheme("window-minimize")));

    auto placement = addRule(new RuleItem(QLatin1String("placement"),
                                          RulePolicy::ForceRule, RuleItem::Option,
                                          i18n("Initial placement"), i18n("Size & Position"),
                                          QIcon::fromTheme("region")));
    placement->setOptionsData(placementModelData());
    placement->setFlag(RuleItem::AffectsWarning);

    auto ignoregeometry = addRule(new RuleItem(QLatin1String("ignoregeometry"),
                                               RulePolicy::SetRule, RuleItem::Boolean,
                                               i18n("Ignore requested geometry"), i18n("Size & Position"),
                                               QIcon::fromTheme("view-time-schedule-baselined-remove"),
                                               xi18nc("@info:tooltip",
                                                      "Some applications can set their own geometry, overriding the window manager preferences. "
                                                      "Setting this property overrides their placement requests."
                                                      "<nl/><nl/>"
                                                      "This affects <interface>Size</interface> and <interface>Position</interface> "
                                                      "but not <interface>Maximized</interface> or <interface>Fullscreen</interface> states."
                                                      "<nl/><nl/>"
                                                      "Note that the position can also be used to map to a different <interface>Screen</interface>")));
    ignoregeometry->setFlag(RuleItem::AffectsWarning);

    addRule(new RuleItem(QLatin1String("minsize"),
                         RulePolicy::ForceRule, RuleItem::Size,
                         i18n("Minimum Size"), i18n("Size & Position"),
                         QIcon::fromTheme("transform-scale")));

    addRule(new RuleItem(QLatin1String("maxsize"),
                         RulePolicy::ForceRule, RuleItem::Size,
                         i18n("Maximum Size"), i18n("Size & Position"),
                         QIcon::fromTheme("transform-scale")));

    addRule(new RuleItem(QLatin1String("strictgeometry"),
                         RulePolicy::ForceRule, RuleItem::Boolean,
                         i18n("Obey geometry restrictions"), i18n("Size & Position"),
                         QIcon::fromTheme("transform-crop-and-resize"),
                         xi18nc("@info:tooltip", "Some apps like video players or terminals can ask KWin to constrain them to "
                                                 "certain aspect ratios or only grow by values larger than the dimensions of one "
                                                 "character. Use this property to ignore such restrictions and allow those windows "
                                                 "to be resized to arbitrary sizes."
                                                 "<nl/><nl/>"
                                                 "This can be helpful for windows that can't quite fit the full screen area when "
                                                 "maximized.")));

    // Arrangement & Access
    addRule(new RuleItem(QLatin1String("above"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Keep above other windows"), i18n("Arrangement & Access"),
                         QIcon::fromTheme("window-keep-above")));

    addRule(new RuleItem(QLatin1String("below"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Keep below other windows"), i18n("Arrangement & Access"),
                         QIcon::fromTheme("window-keep-below")));

    addRule(new RuleItem(QLatin1String("skiptaskbar"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Skip taskbar"), i18n("Arrangement & Access"),
                         QIcon::fromTheme("kt-show-statusbar"),
                         i18nc("@info:tooltip", "Controls whether or not the window appears in the Task Manager.")));

    addRule(new RuleItem(QLatin1String("skippager"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Skip pager"), i18n("Arrangement & Access"),
                         QIcon::fromTheme("org.kde.plasma.pager"),
                         i18nc("@info:tooltip", "Controls whether or not the window appears in the Virtual Desktop manager.")));

    addRule(new RuleItem(QLatin1String("skipswitcher"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("Skip switcher"), i18n("Arrangement & Access"),
                         QIcon::fromTheme("preferences-system-windows-effect-flipswitch"),
                         xi18nc("@info:tooltip", "Controls whether or not the window appears in the <shortcut>Alt+Tab</shortcut> window list.")));

    addRule(new RuleItem(QLatin1String("shortcut"),
                         RulePolicy::SetRule, RuleItem::Shortcut,
                         i18n("Shortcut"), i18n("Arrangement & Access"),
                         QIcon::fromTheme("configure-shortcuts")));

    // Appearance & Fixes
    addRule(new RuleItem(QLatin1String("noborder"),
                         RulePolicy::SetRule, RuleItem::Boolean,
                         i18n("No titlebar and frame"), i18n("Appearance & Fixes"),
                         QIcon::fromTheme("dialog-cancel")));

    auto decocolor = addRule(new RuleItem(QLatin1String("decocolor"),
                                          RulePolicy::ForceRule, RuleItem::Option,
                                          i18n("Titlebar color scheme"), i18n("Appearance & Fixes"),
                                          QIcon::fromTheme("preferences-desktop-theme")));
    decocolor->setOptionsData(colorSchemesModelData());

    auto opacityactive = addRule(new RuleItem(QLatin1String("opacityactive"),
                                              RulePolicy::ForceRule, RuleItem::Percentage,
                                              i18n("Active opacity"), i18n("Appearance & Fixes"),
                                              QIcon::fromTheme("edit-opacity")));
    opacityactive->setFlag(RuleItem::AffectsWarning);
    auto opacityinactive = addRule(new RuleItem(QLatin1String("opacityinactive"),
                                                RulePolicy::ForceRule, RuleItem::Percentage,
                                                i18n("Inactive opacity"), i18n("Appearance & Fixes"),
                                                QIcon::fromTheme("edit-opacity")));
    opacityinactive->setFlag(RuleItem::AffectsWarning);

    auto fsplevel = addRule(new RuleItem(QLatin1String("fsplevel"),
                                         RulePolicy::ForceRule, RuleItem::Option,
                                         i18n("Focus stealing prevention"), i18n("Appearance & Fixes"),
                                         QIcon::fromTheme("preferences-system-windows-effect-glide"),
                                         xi18nc("@info:tooltip", "KWin tries to prevent windows that were opened without direct user action from raising "
                                                                 "themselves and taking focus while you're currently interacting with another window. This "
                                                                 "property can be used to change the level of focus stealing prevention applied to "
                                                                 "individual windows and apps."
                                                                 "<nl/><nl/>"
                                                                 "Here's what will happen to a window opened without your direct action at each level of "
                                                                 "focus stealing prevention:"
                                                                 "<nl/>"
                                                                 "<list>"
                                                                 "<item><emphasis strong='true'>None:</emphasis> The window will be raised and focused.</item>"
                                                                 "<item><emphasis strong='true'>Low:</emphasis> Focus stealing prevention will be applied, "
                                                                 "but in the case of a situation KWin considers ambiguous, the window will be raised and "
                                                                 "focused.</item>"
                                                                 "<item><emphasis strong='true'>Normal:</emphasis> Focus stealing prevention will be "
                                                                 "applied, but  in the case of a situation KWin considers ambiguous, the window will "
                                                                 "<emphasis>not</emphasis> be raised and focused.</item>"
                                                                 "<item><emphasis strong='true'>High:</emphasis> The window will only be raised and focused "
                                                                 "if it belongs to the same app as the currently-focused window.</item>"
                                                                 "<item><emphasis strong='true'>Extreme:</emphasis> The window will never be raised and "
                                                                 "focused.</item>"
                                                                 "</list>")));
    fsplevel->setOptionsData(focusModelData());

    auto fpplevel = addRule(new RuleItem(QLatin1String("fpplevel"),
                                         RulePolicy::ForceRule, RuleItem::Option,
                                         i18n("Focus protection"), i18n("Appearance & Fixes"),
                                         QIcon::fromTheme("preferences-system-windows-effect-minimize"),
                                         xi18nc("@info:tooltip", "This property controls the focus protection level of the currently active "
                                                                 "window. It is used to override the focus stealing prevention applied to new windows that "
                                                                 "are opened without your direct action."
                                                                 "<nl/><nl/>"
                                                                 "Here's what happens to new windows that are opened without your direct action at each "
                                                                 "level of focus protection while the window with this property applied to it has focus:"
                                                                 "<nl/>"
                                                                 "<list>"
                                                                 "<item><emphasis strong='true'>None</emphasis>: Newly-opened windows always raise "
                                                                 "themselves and take focus.</item>"
                                                                 "<item><emphasis strong='true'>Low:</emphasis> Focus stealing prevention will be applied "
                                                                 "to the newly-opened window, but in the case of a situation KWin considers ambiguous, the "
                                                                 "window will be raised and focused.</item>"
                                                                 "<item><emphasis strong='true'>Normal:</emphasis> Focus stealing prevention will be applied "
                                                                 "to the newly-opened window, but in the case of a situation KWin considers ambiguous, the "
                                                                 "window will <emphasis>not</emphasis> be raised and focused.</item>"
                                                                 "<item><emphasis strong='true'>High:</emphasis> Newly-opened windows will only raise "
                                                                 "themselves and take focus if they belongs to the same app as the currently-focused "
                                                                 "window.</item>"
                                                                 "<item><emphasis strong='true'>Extreme:</emphasis> Newly-opened windows never raise "
                                                                 "themselves and take focus.</item>"
                                                                 "</list>")));
    fpplevel->setOptionsData(focusModelData());

    addRule(new RuleItem(QLatin1String("acceptfocus"),
                         RulePolicy::ForceRule, RuleItem::Boolean,
                         i18n("Accept focus"), i18n("Appearance & Fixes"),
                         QIcon::fromTheme("preferences-desktop-cursors"),
                         i18n("Controls whether or not the window becomes focused when clicked.")));

    addRule(new RuleItem(QLatin1String("disableglobalshortcuts"),
                         RulePolicy::ForceRule, RuleItem::Boolean,
                         i18n("Ignore global shortcuts"), i18n("Appearance & Fixes"),
                         QIcon::fromTheme("input-keyboard-virtual-off"),
                         xi18nc("@info:tooltip", "Use this property to prevent global keyboard shortcuts from working while "
                                                 "the window is focused. This can be useful for apps like emulators or virtual "
                                                 "machines that handle some of the same shortcuts themselves."
                                                 "<nl/><nl/>"
                                                 "Note that you won't be able to <shortcut>Alt+Tab</shortcut> out of the window "
                                                 "or use any other global shortcuts such as <shortcut>Alt+Space</shortcut> to "
                                                 "activate KRunner.")));

    addRule(new RuleItem(QLatin1String("closeable"),
                         RulePolicy::ForceRule, RuleItem::Boolean,
                         i18n("Closeable"), i18n("Appearance & Fixes"),
                         QIcon::fromTheme("dialog-close")));

    addRule(new RuleItem(QLatin1String("desktopfile"),
                         RulePolicy::SetRule, RuleItem::String,
                         i18n("Desktop file name"), i18n("Appearance & Fixes"),
                         QIcon::fromTheme("application-x-desktop")));

    addRule(new RuleItem(QLatin1String("blockcompositing"),
                         RulePolicy::ForceRule, RuleItem::Boolean,
                         i18n("Block compositing"), i18n("Appearance & Fixes"),
                         QIcon::fromTheme("composite-track-on")));

    auto layer = addRule(new RuleItem(QLatin1String("layer"),
                                      RulePolicy::ForceRule, RuleItem::Option,
                                      i18n("Layer"), i18n("Appearance & Fixes"),
                                      QIcon::fromTheme("view-sort")));
    layer->setOptionsData(layerModelData());

    addRule(new RuleItem(QLatin1String("adaptivesync"),
                         RulePolicy::ForceRule, RuleItem::Boolean,
                         i18n("Adaptive sync"), i18n("Appearance & Fixes"),
                         QIcon::fromTheme("monitor-symbolic")));

    addRule(new RuleItem(QLatin1String("tearing"),
                         RulePolicy::ForceRule, RuleItem::Boolean,
                         i18n("Allow tearing"), i18n("Appearance & Fixes"),
                         QIcon::fromTheme("monitor-symbolic")));
}

const QHash<QString, QString> RulesModel::x11PropertyHash()
{
    static const auto propertyToRule = QHash<QString, QString>{
        {"caption", "title"},
        {"role", "windowrole"},
        {"clientMachine", "clientmachine"},
        {"maximizeHorizontal", "maximizehoriz"},
        {"maximizeVertical", "maximizevert"},
        {"minimized", "minimize"},
        {"fullscreen", "fullscreen"},
        {"keepAbove", "above"},
        {"keepBelow", "below"},
        {"noBorder", "noborder"},
        {"skipTaskbar", "skiptaskbar"},
        {"skipPager", "skippager"},
        {"skipSwitcher", "skipswitcher"},
        {"desktopFile", "desktopfile"},
        {"desktops", "desktops"},
        {"layer", "layer"},
    };
    return propertyToRule;
};

void RulesModel::setSuggestedProperties(const QVariantMap &info)
{
    // Properties that cannot be directly applied via x11PropertyHash
    const QPoint position = QPoint(info.value("x").toInt(), info.value("y").toInt());
    const QSize size = QSize(info.value("width").toInt(), info.value("height").toInt());

    m_rules["position"]->setSuggestedValue(position);
    m_rules["size"]->setSuggestedValue(size);
    m_rules["minsize"]->setSuggestedValue(size);
    m_rules["maxsize"]->setSuggestedValue(size);

    NET::WindowType window_type = static_cast<NET::WindowType>(info.value("type", 0).toInt());
    if (window_type == NET::Unknown) {
        window_type = NET::Normal;
    }
    m_rules["types"]->setSuggestedValue(1 << window_type);

    const QString wmsimpleclass = info.value("resourceClass").toString();
    const QString wmcompleteclass = QStringLiteral("%1 %2").arg(info.value("resourceName").toString(),
                                                                info.value("resourceClass").toString());

    // This window is not providing the class according to spec (WM_CLASS on X11, appId on Wayland)
    // Notify the user that this is a bug within the application, so there's nothing we can do
    if (wmsimpleclass.isEmpty()) {
        Q_EMIT showErrorMessage(i18n("Window class not available"),
                                xi18nc("@info", "This application is not providing a class for the window, "
                                                "so KWin cannot use it to match and apply any rules. "
                                                "If you still want to apply some rules to it, "
                                                "try to match other properties like the window title instead.<nl/><nl/>"
                                                "Please consider reporting this bug to the application's developers."));
    }

    m_rules["wmclass"]->setSuggestedValue(wmsimpleclass);
    m_rules["wmclasshelper"]->setSuggestedValue(wmcompleteclass);

#if KWIN_BUILD_ACTIVITIES
    const QStringList activities = info.value("activities").toStringList();
    m_rules["activity"]->setSuggestedValue(activities.isEmpty() ? QStringList{Activities::nullUuid()}
                                                                : activities);
#endif

    const auto ruleForProperty = x11PropertyHash();
    for (QString &property : info.keys()) {
        if (!ruleForProperty.contains(property)) {
            continue;
        }
        const QString ruleKey = ruleForProperty.value(property, QString());
        Q_ASSERT(hasRule(ruleKey));

        m_rules[ruleKey]->setSuggestedValue(info.value(property));
    }

    Q_EMIT dataChanged(index(0), index(rowCount() - 1), {RulesModel::SuggestedValueRole});
}

QList<OptionsModel::Data> RulesModel::windowTypesModelData() const
{
    static const auto modelData = QList<OptionsModel::Data>{
        // TODO: Find/create better icons
        {0, i18n("All window types"), {}, {}, OptionsModel::SelectAllOption},
        {1 << NET::Normal, i18n("Normal window"), QIcon::fromTheme("window")},
        {1 << NET::Dialog, i18n("Dialog window"), QIcon::fromTheme("window-duplicate")},
        {1 << NET::Utility, i18n("Utility window"), QIcon::fromTheme("dialog-object-properties")},
        {1 << NET::Dock, i18n("Dock (panel)"), QIcon::fromTheme("value-decrease")}, // see bug 492341
        {1 << NET::Toolbar, i18n("Toolbar"), QIcon::fromTheme("tools")},
        {1 << NET::Menu, i18n("Torn-off menu"), QIcon::fromTheme("overflow-menu-left")},
        {1 << NET::Splash, i18n("Splash screen"), QIcon::fromTheme("embosstool")},
        {1 << NET::Desktop, i18n("Desktop"), QIcon::fromTheme("desktop")},
        // {1 <<  NET::Override, i18n("Unmanaged Window")},  deprecated
        {1 << NET::TopMenu, i18n("Standalone menubar"), QIcon::fromTheme("application-menu")},
        {1 << NET::OnScreenDisplay, i18n("On-screen display"), QIcon::fromTheme("osd-duplicate")}};

    return modelData;
}

QList<OptionsModel::Data> RulesModel::virtualDesktopsModelData() const
{
    QList<OptionsModel::Data> modelData;
    modelData << OptionsModel::Data{
        QString(),
        i18n("All desktops"),
        QIcon::fromTheme("window-pin"),
        i18nc("@info:tooltip in the virtual desktop list", "Make the window available on all desktops"),
        OptionsModel::ExclusiveOption,
    };
    for (const DBusDesktopDataStruct &desktop : m_virtualDesktops) {
        modelData << OptionsModel::Data{
            desktop.id,
            QString::number(desktop.position + 1).rightJustified(2) + QStringLiteral(": ") + desktop.name,
            QIcon::fromTheme("virtual-desktops")};
    }
    return modelData;
}

QList<OptionsModel::Data> RulesModel::activitiesModelData() const
{
#if KWIN_BUILD_ACTIVITIES
    QList<OptionsModel::Data> modelData;

    modelData << OptionsModel::Data{
        Activities::nullUuid(),
        i18n("All activities"),
        QIcon::fromTheme("activities"),
        i18nc("@info:tooltip in the activity list", "Make the window available on all activities"),
        OptionsModel::ExclusiveOption,
    };

    const auto activities = m_activities->activities();
    if (m_activities->serviceStatus() == KActivities::Consumer::Running) {
        for (const QString &activityId : activities) {
            const KActivities::Info info(activityId);
            modelData << OptionsModel::Data{activityId, info.name(), QIcon::fromTheme(info.icon())};
        }
    }

    return modelData;
#else
    return {};
#endif
}

QList<OptionsModel::Data> RulesModel::placementModelData() const
{
    static const auto modelData = QList<OptionsModel::Data>{
        {PlacementDefault, i18n("Default")},
        {PlacementNone, i18n("No placement")},
        {PlacementSmart, i18n("Minimal overlapping")},
        {PlacementMaximizing, i18n("Maximized")},
        {PlacementCentered, i18n("Centered")},
        {PlacementRandom, i18n("Random")},
        {PlacementZeroCornered, i18n("In top-left corner")},
        {PlacementUnderMouse, i18n("Under mouse")},
        {PlacementOnMainWindow, i18n("On main window")}};
    return modelData;
}

QList<OptionsModel::Data> RulesModel::focusModelData() const
{
    static const auto modelData = QList<OptionsModel::Data>{
        {0, i18n("None")},
        {1, i18n("Low")},
        {2, i18n("Normal")},
        {3, i18n("High")},
        {4, i18n("Extreme")}};
    return modelData;
}

QList<OptionsModel::Data> RulesModel::colorSchemesModelData() const
{
    QList<OptionsModel::Data> modelData;

    QAbstractItemModel *schemesModel = KColorSchemeManager::instance()->model();

    // Skip row 0, which is Default scheme
    for (int r = 1; r < schemesModel->rowCount(); r++) {
        const QModelIndex index = schemesModel->index(r, 0);
        modelData << OptionsModel::Data{
            QFileInfo(index.data(Qt::UserRole).toString()).baseName(),
            index.data(Qt::DisplayRole).toString(),
            index.data(Qt::DecorationRole).value<QIcon>()};
    }

    return modelData;
}

QList<OptionsModel::Data> RulesModel::layerModelData() const
{
    static const auto modelData = QList<OptionsModel::Data>{
        {DesktopLayer, i18n("Desktop")},
        {BelowLayer, i18n("Below")},
        {NormalLayer, i18n("Normal")},
        {AboveLayer, i18n("Above")},
        {NotificationLayer, i18n("Notification")},
        {ActiveLayer, i18n("Fullscreen")},
        {PopupLayer, i18n("Popup")},
        {CriticalNotificationLayer, i18n("Critical notification")},
        {OnScreenDisplayLayer, i18n("On-screen display")},
        {OverlayLayer, i18n("Overlay")},
    };
    return modelData;
}

void RulesModel::detectWindowProperties(int miliseconds)
{
    QTimer::singleShot(miliseconds, this, &RulesModel::selectX11Window);
}

void RulesModel::selectX11Window()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/KWin"),
                                                          QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("queryWindowInfo"));

    QDBusPendingReply<QVariantMap> async = QDBusConnection::sessionBus().asyncCall(message);

    QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(async, this);
    connect(callWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<QVariantMap> reply = *self;
        self->deleteLater();
        if (!reply.isValid()) {
            if (reply.error().name() == QLatin1String("org.kde.KWin.Error.InvalidWindow")) {
                Q_EMIT showErrorMessage(i18n("Unmanaged window"),
                                        i18n("Could not detect window properties. The window is not managed by KWin."));
            }
            return;
        }
        const QVariantMap windowInfo = reply.value();
        setSuggestedProperties(windowInfo);
        Q_EMIT showSuggestions();
    });
}

void RulesModel::updateVirtualDesktops()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/VirtualDesktopManager"),
                                                          QStringLiteral("org.freedesktop.DBus.Properties"),
                                                          QStringLiteral("Get"));
    message.setArguments(QVariantList{
        QStringLiteral("org.kde.KWin.VirtualDesktopManager"),
        QStringLiteral("desktops")});

    QDBusPendingReply<QVariant> async = QDBusConnection::sessionBus().asyncCall(message);

    QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(async, this);
    connect(callWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<QVariant> reply = *self;
        self->deleteLater();
        if (!reply.isValid()) {
            return;
        }
        m_virtualDesktops = qdbus_cast<KWin::DBusDesktopDataVector>(reply.value());
        Q_EMIT virtualDesktopsUpdated();
    });
}

} // namespace

#include "moc_rulesmodel.cpp"
