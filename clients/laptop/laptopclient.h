/*
 * Laptop KWin Client
 *
 * Copyright (c) 2005 Sandro Giessl <sandro@giessl.com>
 * Ported to the kde3.2 API by Luciano Montanaro <mikelima@cirulla.net>
 */
#ifndef __KDECLIENT_H
#define __KDECLIENT_H

#include <qbitmap.h>
#include <kpixmap.h>
#include <kcommondecoration.h>
#include <kdecorationfactory.h>

namespace Laptop {

class LaptopClient;

class LaptopButton : public KCommonDecorationButton
{
public:
    LaptopButton(ButtonType type, LaptopClient *parent, const char *name);
    void setBitmap(const unsigned char *bitmap);
    virtual void reset(unsigned long changed);

protected:
    void paintEvent(QPaintEvent *);
    virtual void drawButton(QPainter *p);
    QBitmap deco;
};

class LaptopClient : public KCommonDecoration
{
public:
    LaptopClient( KDecorationBridge* b, KDecorationFactory* f );
    ~LaptopClient();

    virtual QString visibleName() const;
    virtual QString defaultButtonsLeft() const;
    virtual QString defaultButtonsRight() const;
    virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;
    virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;
    virtual KCommonDecorationButton *createButton(ButtonType type);

    virtual QRegion cornerShape(WindowCorner corner);

    void init();
protected:
    void paintEvent( QPaintEvent* );
    void reset( unsigned long );
    void updateActiveBuffer();
private:
    bool mustDrawHandle() const;
    bool isTransient() const;
private:
    KPixmap activeBuffer;
    int lastBufferWidth;
    bool bufferDirty;
};

class LaptopClientFactory : public QObject, public KDecorationFactory
{
public:
    LaptopClientFactory();
    virtual ~LaptopClientFactory();
    virtual KDecoration* createDecoration( KDecorationBridge* );
    virtual bool reset( unsigned long changed );
    virtual bool supports( Ability ability );
    virtual QList< BorderSize > borderSizes() const;
private:
    void findPreferredHandleSize();
};

}

#endif
