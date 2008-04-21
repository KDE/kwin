/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Cédric Borgese <cedric.borgese@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "wobblywindows_config.h"
#include "wobblywindows_constants.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KGlobalAccel>
#include <kconfiggroup.h>

#include <QGridLayout>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

WobblyWindowsEffectConfig::WobblyWindowsEffectConfig(QWidget* parent, const QVariantList& args) :
KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui.setupUi(this);

    connect(m_ui.spStiffness, SIGNAL(valueChanged(double)), this, SLOT(slotSpStiffness(double)));
    connect(m_ui.slStiffness, SIGNAL(sliderMoved(int)), this, SLOT(slotSlStiffness(int)));
    connect(m_ui.spDrag, SIGNAL(valueChanged(double)), this, SLOT(slotSpDrag(double)));
    connect(m_ui.slDrag, SIGNAL(sliderMoved(int)), this, SLOT(slotSlDrag(int)));
    connect(m_ui.spMovFactor, SIGNAL(valueChanged(double)), this, SLOT(slotSpMovFactor(double)));
    connect(m_ui.slMovFactor, SIGNAL(sliderMoved(int)), this, SLOT(slotSlMovFactor(int)));

    connect(m_ui.cbGridFilter, SIGNAL(activated(int)), this, SLOT(slotGridParameterSelected(int)));

    connect(m_ui.rbNone, SIGNAL(toggled(bool)), this, SLOT(slotRbNone(bool)));
    connect(m_ui.rbFourRingMean, SIGNAL(toggled(bool)), this, SLOT(slotRbFourRingMean(bool)));
    connect(m_ui.rbHeightRingMean, SIGNAL(toggled(bool)), this, SLOT(slotRbHeightRingMean(bool)));
    connect(m_ui.rbMeanMean, SIGNAL(toggled(bool)), this, SLOT(slotRbMeanMean(bool)));
    connect(m_ui.rbMeanMedian, SIGNAL(toggled(bool)), this, SLOT(slotRbMeanMedian(bool)));

    connect(m_ui.spMinVel, SIGNAL(valueChanged(double)), this, SLOT(slotSpMinVel(double)));
    connect(m_ui.slMinVel, SIGNAL(sliderMoved(int)), this, SLOT(slotSlMinVel(int)));
    connect(m_ui.spMaxVel, SIGNAL(valueChanged(double)), this, SLOT(slotSpMaxVel(double)));
    connect(m_ui.slMaxVel, SIGNAL(sliderMoved(int)), this, SLOT(slotSlMaxVel(int)));
    connect(m_ui.spStopVel, SIGNAL(valueChanged(double)), this, SLOT(slotSpStopVel(double)));
    connect(m_ui.slStopVel, SIGNAL(sliderMoved(int)), this, SLOT(slotSlStopVel(int)));
    connect(m_ui.spMinAcc, SIGNAL(valueChanged(double)), this, SLOT(slotSpMinAcc(double)));
    connect(m_ui.slMinAcc, SIGNAL(sliderMoved(int)), this, SLOT(slotSlMinAcc(int)));
    connect(m_ui.spMaxAcc, SIGNAL(valueChanged(double)), this, SLOT(slotSpMaxAcc(double)));
    connect(m_ui.slMaxAcc, SIGNAL(sliderMoved(int)), this, SLOT(slotSlMaxAcc(int)));
    connect(m_ui.spStopAcc, SIGNAL(valueChanged(double)), this, SLOT(slotSpStopAcc(double)));
    connect(m_ui.slStopAcc, SIGNAL(sliderMoved(int)), this, SLOT(slotSlStopAcc(int)));

    load();
}

WobblyWindowsEffectConfig::~WobblyWindowsEffectConfig()
{
}

void WobblyWindowsEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Wobbly");
    qreal stiffness = conf.readEntry("Stiffness", STIFFNESS);
    qreal drag = conf.readEntry("Drag", DRAG);
    qreal move_factor = conf.readEntry("MoveFactor", MOVEFACTOR);

    m_ui.spStiffness->setValue(stiffness);
    m_ui.slStiffness->setSliderPosition(stiffness*50);

    m_ui.spDrag->setValue(drag);
    m_ui.slDrag->setSliderPosition(drag*100);

    m_ui.spMovFactor->setValue(move_factor);
    m_ui.slMovFactor->setValue(move_factor*100);

    int xTesselation = conf.readEntry("XTesselation", XTESSELATION);
    int yTesselation = conf.readEntry("YTesselation", YTESSELATION);

    m_ui.spHNodes->setValue(xTesselation);
    m_ui.spVNodes->setValue(yTesselation);

    //squareRootMasterAcceleration = conf.readEntry("SquareRootMasterAcceleration", false);

    QString velFilter = conf.readEntry("VelocityFilter", VELOCITYFILTER);
    if (velFilter == "NoFilter")
    {
        velocityFilter = NoFilter;
    }
    else if (velFilter == "FourRingLinearMean")
    {
        velocityFilter = FourRingLinearMean;
    }
    else if (velFilter == "HeightRingLinearMean")
    {
        velocityFilter = HeightRingLinearMean;
    }
    else if (velFilter == "MeanWithMean")
    {
        velocityFilter = MeanWithMean;
    }
    else if (velFilter == "MeanWithMedian")
    {
        velocityFilter = MeanWithMedian;
    }
    else
    {
        velocityFilter = FourRingLinearMean;
        kDebug() << "Unknown config value for VelocityFilter : " << velFilter;
    }


    QString accFilter = conf.readEntry("AccelerationFilter", ACCELERATIONFILTER);
    if (accFilter == "NoFilter")
    {
        accelerationFilter = NoFilter;
    }
    else if (accFilter == "FourRingLinearMean")
    {
        accelerationFilter = FourRingLinearMean;
    }
    else if (accFilter == "HeightRingLinearMean")
    {
        accelerationFilter = HeightRingLinearMean;
    }
    else if (accFilter == "MeanWithMean")
    {
        accelerationFilter = MeanWithMean;
    }
    else if (accFilter == "MeanWithMedian")
    {
        accelerationFilter = MeanWithMedian;
    }
    else
    {
        accelerationFilter = NoFilter;
        kDebug() << "Unknown config value for accelerationFilter : " << accFilter;
    }

    qreal minVel = conf.readEntry("MinVelocity", MINVELOCITY);
    qreal maxVel = conf.readEntry("MaxVelocity", MAXVELOCITY);
    qreal stopVel = conf.readEntry("StopVelocity", STOPVELOCITY);
    qreal minAcc = conf.readEntry("MinAcceleration", MINACCELERATION);
    qreal maxAcc = conf.readEntry("MaxAcceleration", MAXACCELERATION);
    qreal stopAcc = conf.readEntry("StopAcceleration", STOPACCELERATION);

    m_ui.spMinVel->setValue(minVel);
    m_ui.slMinVel->setSliderPosition(minVel*100);
    m_ui.spMaxVel->setValue(maxVel);
    m_ui.slMaxVel->setSliderPosition(maxVel/10);
    m_ui.spStopVel->setValue(stopVel);
    m_ui.slStopVel->setSliderPosition(stopVel*10);
    m_ui.spMinAcc->setValue(minAcc);
    m_ui.slMinAcc->setSliderPosition(minAcc*100);
    m_ui.spMaxAcc->setValue(maxAcc);
    m_ui.slMaxAcc->setSliderPosition(maxAcc/10);
    m_ui.spStopAcc->setValue(stopAcc);
    m_ui.slStopAcc->setSliderPosition(stopAcc*10);

    emit changed(false);
}

void WobblyWindowsEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("Wobbly");

    conf.writeEntry("Stiffness", m_ui.spStiffness->value());
    conf.writeEntry("Drag", m_ui.spDrag->value());
    conf.writeEntry("MoveFactor", m_ui.spMovFactor->value());

    conf.writeEntry("XTesselation", m_ui.spHNodes->value());
    conf.writeEntry("YTesselation", m_ui.spVNodes->value());

    switch (velocityFilter)
    {
    case NoFilter:
        conf.writeEntry("VelocityFilter", "NoFilter");
        break;

    case FourRingLinearMean:
        conf.writeEntry("VelocityFilter", "FourRingLinearMean");
        break;

    case HeightRingLinearMean:
        conf.writeEntry("VelocityFilter", "HeightRingLinearMean");
        break;

    case MeanWithMean:
        conf.writeEntry("VelocityFilter", "MeanWithMean");
        break;

    case MeanWithMedian:
        conf.writeEntry("VelocityFilter", "MeanWithMedian");
        break;
    }

    switch (accelerationFilter)
    {
    case NoFilter:
        conf.writeEntry("AccelerationFilter", "NoFilter");
        break;

    case FourRingLinearMean:
        conf.writeEntry("AccelerationFilter", "FourRingLinearMean");
        break;

    case HeightRingLinearMean:
        conf.writeEntry("AccelerationFilter", "HeightRingLinearMean");
        break;

    case MeanWithMean:
        conf.writeEntry("AccelerationFilter", "MeanWithMean");
        break;

    case MeanWithMedian:
        conf.writeEntry("AccelerationFilter", "MeanWithMedian");
        break;
   }

    conf.writeEntry("MinVelocity", m_ui.spMinVel->value());
    conf.writeEntry("MaxVelocity", m_ui.spMaxVel->value());
    conf.writeEntry("StopVelocity", m_ui.spStopVel->value());
    conf.writeEntry("MinAcceleration", m_ui.spMinAcc->value());
    conf.writeEntry("MaxAcceleration", m_ui.spMaxAcc->value());
    conf.writeEntry("StopAcceleration", m_ui.spStopAcc->value());

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("kwin4_effect_wobblywindows");
}

void WobblyWindowsEffectConfig::defaults()
{
    m_ui.spStiffness->setValue(STIFFNESS);
    m_ui.slStiffness->setSliderPosition(STIFFNESS*50);    

    m_ui.spDrag->setValue(DRAG);
    m_ui.slDrag->setSliderPosition(DRAG*100);

    m_ui.spMovFactor->setValue(MOVEFACTOR);
    m_ui.slMovFactor->setValue(MOVEFACTOR*100);

    m_ui.spHNodes->setValue(XTESSELATION);
    m_ui.spVNodes->setValue(YTESSELATION);

    velocityFilter = FourRingLinearMean;
    accelerationFilter = NoFilter;
    slotGridParameterSelected(m_ui.cbGridFilter->currentIndex());

    m_ui.spMinVel->setValue(MINVELOCITY);
    m_ui.slMinVel->setSliderPosition(MINVELOCITY*100);

    m_ui.spMaxVel->setValue(MAXVELOCITY);
    m_ui.slMaxVel->setSliderPosition(MAXVELOCITY/10);

    m_ui.spStopVel->setValue(STOPVELOCITY);
    m_ui.slStopVel->setSliderPosition(STOPVELOCITY*10);

    m_ui.spMinAcc->setValue(MINACCELERATION);
    m_ui.slMinAcc->setSliderPosition(MINACCELERATION*100);

    m_ui.spMaxAcc->setValue(MAXACCELERATION);
    m_ui.slMaxAcc->setSliderPosition(MAXACCELERATION/10);

    m_ui.spStopAcc->setValue(STOPACCELERATION);
    m_ui.slStopAcc->setSliderPosition(STOPACCELERATION*10);

    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSpStiffness(double value)
{
    m_ui.slStiffness->setSliderPosition(value*50);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlStiffness(int value)
{
    m_ui.spStiffness->setValue(value/50.0);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSpDrag(double value)
{
    m_ui.slDrag->setSliderPosition(value*100);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlDrag(int value)
{
    m_ui.spDrag->setValue(qreal(value)/100.0);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSpMovFactor(double value)
{
    m_ui.slMovFactor->setValue(value*100);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlMovFactor(int value)
{
    m_ui.spMovFactor->setValue(qreal(value)/100.0);
    emit changed(true);
}

// filters

void WobblyWindowsEffectConfig::slotRbNone(bool toggled)
{
    if (toggled)
    {
        if (m_ui.cbGridFilter->currentIndex() == 0) // velocity
        {
            velocityFilter = NoFilter;
        }
        else if (m_ui.cbGridFilter->currentIndex() == 1) // acceleration
        {
            accelerationFilter = NoFilter;
        }
    }
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotRbFourRingMean(bool toggled)
{
    if (toggled)
    {
        if (m_ui.cbGridFilter->currentIndex() == 0) // velocity
        {
            velocityFilter = FourRingLinearMean;
        }
        else if (m_ui.cbGridFilter->currentIndex() == 1) // acceleration
        {
            accelerationFilter = FourRingLinearMean;
        }
    }
    emit changed(true);
}


void WobblyWindowsEffectConfig::slotRbHeightRingMean(bool toggled)
{
    if (toggled)
    {
        if (m_ui.cbGridFilter->currentIndex() == 0) // velocity
        {
            velocityFilter = HeightRingLinearMean;
        }
        else if (m_ui.cbGridFilter->currentIndex() == 1) // acceleration
        {
            accelerationFilter = HeightRingLinearMean;
        }
    }
    emit changed(true);
}


void WobblyWindowsEffectConfig::slotRbMeanMean(bool toggled)
{
    if (toggled)
    {
        if (m_ui.cbGridFilter->currentIndex() == 0) // velocity
        {
            velocityFilter = MeanWithMean;
        }
        else if (m_ui.cbGridFilter->currentIndex() == 1) // acceleration
        {
            accelerationFilter = MeanWithMean;
        }
    }
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotRbMeanMedian(bool toggled)
{
    if (toggled)
    {
        if (m_ui.cbGridFilter->currentIndex() == 0) // velocity
        {
            velocityFilter = MeanWithMedian;
        }
        else if (m_ui.cbGridFilter->currentIndex() == 1) // acceleration
        {
            accelerationFilter = MeanWithMedian;
        }
    }
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotGridParameterSelected(int index)
{
    if (index == 0) // velocity
    {
        switch (velocityFilter)
        {
        case NoFilter:
            m_ui.rbNone->setChecked(true);
            break;

        case FourRingLinearMean:
            m_ui.rbFourRingMean->setChecked(true);
            break;

        case HeightRingLinearMean:
            m_ui.rbHeightRingMean->setChecked(true);
            break;

        case MeanWithMean:
            m_ui.rbMeanMean->setChecked(true);
            break;

        case MeanWithMedian:
            m_ui.rbMeanMedian->setChecked(true);
            break;
        }
    }
    else if (index == 1) // acceleration
    {
        switch (accelerationFilter)
        {
        case NoFilter:
            m_ui.rbNone->setChecked(true);
            break;

        case FourRingLinearMean:
            m_ui.rbFourRingMean->setChecked(true);
            break;

        case HeightRingLinearMean:
            m_ui.rbHeightRingMean->setChecked(true);
            break;

        case MeanWithMean:
            m_ui.rbMeanMean->setChecked(true);
            break;

        case MeanWithMedian:
            m_ui.rbMeanMedian->setChecked(true);
            break;
       }
    }
    emit changed(true);
}

// thresholds

void WobblyWindowsEffectConfig::slotSpMinVel(double value)
{
    m_ui.slMinVel->setSliderPosition(value*100);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlMinVel(int value)
{
    m_ui.spMinVel->setValue(qreal(value)/100.0);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSpMaxVel(double value)
{
    m_ui.slMaxVel->setSliderPosition(value/10);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlMaxVel(int value)
{
    m_ui.spMaxVel->setValue(value*10.0);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSpStopVel(double value)
{
    m_ui.slStopVel->setSliderPosition(value*10);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlStopVel(int value)
{
    m_ui.spStopVel->setValue(value/10.0);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSpMinAcc(double value)
{
    m_ui.slMinAcc->setSliderPosition(value*100);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlMinAcc(int value)
{
    m_ui.spMinAcc->setValue(value/100.0);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSpMaxAcc(double value)
{
    m_ui.slMaxAcc->setSliderPosition(value/10);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlMaxAcc(int value)
{
    m_ui.spMaxAcc->setValue(value*10.0);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSpStopAcc(double value)
{
    m_ui.slStopAcc->setSliderPosition(value*10);
    emit changed(true);
}

void WobblyWindowsEffectConfig::slotSlStopAcc(int value)
{
    m_ui.spStopAcc->setValue(value/10.0);
    emit changed(true);
}


} // namespace

#include "wobblywindows_config.moc"
