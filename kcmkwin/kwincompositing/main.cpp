/**************************************************************************
 * KWin - the KDE window manager                                          *
 * This file is part of the KDE project.                                  *
 *                                                                        *
 * Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
 * Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>                 *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 2 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/


#include "compositing.h"
#include "model.h"
#include "ui_compositing.h"
#include <QApplication>
#include <QLayout>

#include <kcmodule.h>
#include <kservice.h>

class KWinCompositingKCM : public KCModule
{
    Q_OBJECT
public:
    virtual ~KWinCompositingKCM();

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

protected:
    explicit KWinCompositingKCM(QWidget* parent, const QVariantList& args,
                                KWin::Compositing::EffectView::ViewType viewType);

private:
    QScopedPointer<KWin::Compositing::EffectView> m_view;
};

class KWinDesktopEffects : public KWinCompositingKCM
{
    Q_OBJECT
public:
    explicit KWinDesktopEffects(QWidget* parent = 0, const QVariantList& args = QVariantList())
        : KWinCompositingKCM(parent, args, KWin::Compositing::EffectView::DesktopEffectsView) {}
};

class KWinCompositingSettings : public KCModule
{
    Q_OBJECT
public:
    explicit KWinCompositingSettings(QWidget *parent = 0, const QVariantList &args = QVariantList());

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private:
    void init();
    KWin::Compositing::Compositing *m_compositing;
    Ui_CompositingForm m_form;
};

KWinCompositingSettings::KWinCompositingSettings(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_compositing(new KWin::Compositing::Compositing(this))
{
    m_form.setupUi(this);
    m_form.glCrashedWarning->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    QAction *reenableGLAction = new QAction(i18n("Re-enable OpenGL detection"), this);
    connect(reenableGLAction, &QAction::triggered, m_compositing, &KWin::Compositing::Compositing::reenableOpenGLDetection);
    connect(reenableGLAction, &QAction::triggered, m_form.glCrashedWarning, &KMessageWidget::animatedHide);
    m_form.glCrashedWarning->addAction(reenableGLAction);
    m_form.scaleWarning->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    m_form.tearingWarning->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    m_form.windowThumbnailWarning->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    m_form.unredirectInformation->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information")));

    init();
}

void KWinCompositingSettings::init()
{
    using namespace KWin::Compositing;
    auto currentIndexChangedSignal = static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged);

    connect(m_compositing, &Compositing::changed, this, static_cast<void(KCModule::*)()>(&KWinCompositingSettings::changed));

    // enabled check box
    m_form.compositingEnabled->setChecked(m_compositing->compositingEnabled());
    connect(m_compositing, &Compositing::compositingEnabledChanged, m_form.compositingEnabled, &QCheckBox::setChecked);
    connect(m_form.compositingEnabled, &QCheckBox::toggled, m_compositing, &Compositing::setCompositingEnabled);

    // animation speed
    m_form.animationSpeed->setValue(m_compositing->animationSpeed());
    connect(m_compositing, &Compositing::animationSpeedChanged, m_form.animationSpeed, &QSlider::setValue);
    connect(m_form.animationSpeed, &QSlider::valueChanged, m_compositing, &Compositing::setAnimationSpeed);

    // gl scale filter
    m_form.glScaleFilter->setCurrentIndex(m_compositing->glScaleFilter());
    connect(m_compositing, &Compositing::glScaleFilterChanged, m_form.glScaleFilter, &QComboBox::setCurrentIndex);
    connect(m_form.glScaleFilter, currentIndexChangedSignal, m_compositing, &Compositing::setGlScaleFilter);
    connect(m_form.glScaleFilter, currentIndexChangedSignal,
        [this](int index) {
            if (index == 2) {
                m_form.scaleWarning->animatedShow();
            } else {
                m_form.scaleWarning->animatedHide();
            }
        }
    );

    // xrender scale filter
    m_form.xrScaleFilter->setCurrentIndex(m_compositing->xrScaleFilter());
    connect(m_compositing, &Compositing::xrScaleFilterChanged, m_form.xrScaleFilter, &QComboBox::setCurrentIndex);
    connect(m_form.xrScaleFilter, currentIndexChangedSignal, m_compositing, &Compositing::setXrScaleFilter);

    // tearing prevention
    m_form.tearingPrevention->setCurrentIndex(m_compositing->glSwapStrategy());
    connect(m_compositing, &Compositing::glSwapStrategyChanged, m_form.tearingPrevention, &QComboBox::setCurrentIndex);
    connect(m_form.tearingPrevention, currentIndexChangedSignal, m_compositing, &Compositing::setGlSwapStrategy);
    connect(m_form.tearingPrevention, currentIndexChangedSignal,
        [this](int index) {
            if (index == 2) {
                // only when cheap - tearing
                m_form.tearingWarning->setText(i18n("\"Only when cheap\" only prevents tearing for full screen changes like a video."));
                m_form.tearingWarning->animatedShow();
            } else if (index == 3) {
                // full screen repaints
                m_form.tearingWarning->setText(i18n("\"Full screen repaints\" can cause performance problems."));
                m_form.tearingWarning->animatedShow();
            } else if (index == 4) {
                // re-use screen content
                m_form.tearingWarning->setText(i18n("\"Re-use screen content\" causes severe performance problems on MESA drivers."));
                m_form.tearingWarning->animatedShow();
            } else {
                m_form.tearingWarning->animatedHide();
            }
        }
    );

    // windowThumbnail
    m_form.windowThumbnail->setCurrentIndex(m_compositing->windowThumbnail());
    connect(m_compositing, &Compositing::windowThumbnailChanged, m_form.windowThumbnail, &QComboBox::setCurrentIndex);
    connect(m_form.windowThumbnail, currentIndexChangedSignal, m_compositing, &Compositing::setWindowThumbnail);
    connect(m_form.windowThumbnail, currentIndexChangedSignal,
        [this](int index) {
            if (index == 2) {
                m_form.windowThumbnailWarning->animatedShow();
            } else {
                m_form.windowThumbnailWarning->animatedHide();
            }
        }
    );

    // unredirect fullscreen
    m_form.unredirectFullscreen->setChecked(m_compositing->unredirectFullscreen());
    connect(m_compositing, &Compositing::unredirectFullscreenChanged, m_form.unredirectFullscreen, &QCheckBox::setChecked);
    connect(m_form.unredirectFullscreen, &QCheckBox::toggled, m_compositing, &Compositing::setUnredirectFullscreen);
    connect(m_form.unredirectFullscreen, &QCheckBox::toggled,
        [this](bool enabled) {
            if (enabled) {
                m_form.unredirectInformation->animatedShow();
            } else {
                m_form.unredirectInformation->animatedHide();
            }
        }
    );

    // color correction
    m_form.colorCorrection->setChecked(m_compositing->glColorCorrection());
    connect(m_compositing, &Compositing::glColorCorrectionChanged, m_form.colorCorrection, &QCheckBox::setChecked);
    connect(m_form.colorCorrection, &QCheckBox::toggled, m_compositing, &Compositing::setGlColorCorrection);

    // compositing type
    CompositingType *type = new CompositingType(this);
    m_form.type->setModel(type);
    auto updateCompositingType = [this, type]() {
        m_form.type->setCurrentIndex(type->indexForCompositingType(m_compositing->compositingType()));
    };
    updateCompositingType();
    connect(m_compositing, &Compositing::compositingTypeChanged,
        [updateCompositingType]() {
            updateCompositingType();
        }
    );
    auto showHideBasedOnType = [this, type]() {
        const int currentType = type->compositingTypeForIndex(m_form.type->currentIndex());
        m_form.glScaleFilter->setVisible(currentType != CompositingType::XRENDER_INDEX);
        m_form.glScaleFilterLabel->setVisible(currentType != CompositingType::XRENDER_INDEX);
        m_form.xrScaleFilter->setVisible(currentType == CompositingType::XRENDER_INDEX);
        m_form.xrScaleFilterLabel->setVisible(currentType == CompositingType::XRENDER_INDEX);
        m_form.colorCorrection->setEnabled(currentType == CompositingType::OPENGL31_INDEX || currentType == CompositingType::OPENGL20_INDEX);
    };
    showHideBasedOnType();
    connect(m_form.type, currentIndexChangedSignal,
        [this, type, showHideBasedOnType]() {
            m_compositing->setCompositingType(type->compositingTypeForIndex(m_form.type->currentIndex()));
            showHideBasedOnType();
        }
    );

    if (m_compositing->OpenGLIsUnsafe()) {
        m_form.glCrashedWarning->animatedShow();
    }
}

void KWinCompositingSettings::load()
{
    KCModule::load();
    m_compositing->reset();
}

void KWinCompositingSettings::defaults()
{
    KCModule::defaults();
    m_compositing->defaults();
}

void KWinCompositingSettings::save()
{
    KCModule::save();
    m_compositing->save();
}

KWinCompositingKCM::KWinCompositingKCM(QWidget* parent, const QVariantList& args, KWin::Compositing::EffectView::ViewType viewType)
    : KCModule(parent, args)
    , m_view(new KWin::Compositing::EffectView(viewType))
{
    QVBoxLayout *vl = new QVBoxLayout(this);

    QWidget *w = QWidget::createWindowContainer(m_view.data(), this);
    connect(m_view.data(), &QWindow::minimumWidthChanged, w, &QWidget::setMinimumWidth);
    connect(m_view.data(), &QWindow::minimumHeightChanged, w, &QWidget::setMinimumHeight);
    w->setMinimumSize(m_view->initialSize());
    vl->addWidget(w);
    setLayout(vl);
    connect(m_view.data(), &KWin::Compositing::EffectView::changed, [this]{
        emit changed(true);
    });
    w->setFocusPolicy(Qt::StrongFocus);
}

KWinCompositingKCM::~KWinCompositingKCM()
{
}

void KWinCompositingKCM::save()
{
    m_view->save();
    KCModule::save();
}

void KWinCompositingKCM::load()
{
    m_view->load();
    KCModule::load();
}

void KWinCompositingKCM::defaults()
{
    m_view->defaults();
    KCModule::defaults();
}

K_PLUGIN_FACTORY(KWinCompositingConfigFactory,
                 registerPlugin<KWinDesktopEffects>("effects");
                 registerPlugin<KWinCompositingSettings>("compositing");
                )

#include "main.moc"
