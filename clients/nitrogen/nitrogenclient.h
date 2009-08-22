#ifndef nitrogenclient_h
#define nitrogenclient_h

// $Id: nitrogenclient.h,v 1.13 2009/07/05 17:56:25 hpereira Exp $


/******************************************************************************
*                        
* Copyright (C) 2002 Hugo PEREIRA <mailto: hugo.pereira@free.fr>            
*                        
* This is free software; you can redistribute it and/or modify it under the     
* terms of the GNU General Public License as published by the Free Software     
* Foundation; either version 2 of the License, or (at your option) any later   
* version.                            
*                         
* This software is distributed in the hope that it will be useful, but WITHOUT 
* ANY WARRANTY;  without even the implied warranty of MERCHANTABILITY or         
* FITNESS FOR A PARTICULAR PURPOSE.   See the GNU General Public License         
* for more details.                    
*                         
* You should have received a copy of the GNU General Public License along with 
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     
* Place, Suite 330, Boston, MA   02111-1307 USA                          
*                        
*                        
*******************************************************************************/

#include <kcommondecoration.h>

#include "kdeversion.h"
#include "nitrogenconfiguration.h"
#include "lib/helper.h"
#include "lib/tileset.h"


namespace Nitrogen 
{
    
    class NitrogenSizeGrip;
    
    class NitrogenClient : public KCommonDecorationUnstable
    {
        
        Q_OBJECT
            
            public:
            
            //! constructor
            NitrogenClient(KDecorationBridge *b, KDecorationFactory *f);
        
        //! destructor
        virtual ~NitrogenClient();
        
        virtual QString visibleName() const;
        virtual KCommonDecorationButton *createButton(::ButtonType type);
        virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;
        
        //! true if window is maximized
        virtual bool isMaximized( void ) const;
        
        //! dimensions
        virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;
        
        //! border width
        /*! full border width, left and right */
        virtual int borderWidth( void ) const;
        
        //! border height
        /*! full border height, top and bottom */
        virtual int borderHeight( void ) const;
        
        //! window shape
        virtual void updateWindowShape();
        
        //! initialization
        virtual void init();
        
        #if !KDE_IS_VERSION(4,2,92)
        virtual QList<QRect> shadowQuads( ShadowType type ) const;
        virtual double shadowOpacity( ShadowType type ) const;
        #endif
        
        //! return associated configuration
        NitrogenConfiguration configuration( void ) const;
        
        //! helper class
        NitrogenHelper& helper( void ) const
        { return helper_; }
        
        //! window background
        virtual void renderWindowBackground( QPainter*, const QRect&, const QWidget*, const QPalette& ) const;
        
        //! triggered when window activity is changed
        virtual void activeChange();
        
        public slots:
        
        //! reset configuration
        void resetConfiguration( void );
        
        protected:
        
        //! paint
        void paintEvent( QPaintEvent* );
        
        //! shadows
        TileSet *shadowTiles(const QColor& color, const QColor& glow, qreal size, bool active);
        
        private:
        
        struct ShadowTilesOption {
            QColor windowColor;
            QColor glowColor;
            qreal width;
            bool active;
        };
       
        //! palette background
        QPalette backgroundPalette( const QWidget*, QPalette ) const;
        
        //! draw
        void drawStripes(QPainter*, QPalette&, const int, const int, const int);
        
        //! calculate mask
        QRegion calcMask( void ) const;
        
        //! text color
        QColor titlebarTextColor(const QPalette&);
        
        //!@name size grip
        //@{
        
        //! create size grip
        void createSizeGrip( void );
        
        //! delete size grip
        void deleteSizeGrip( void );
        
        // size grip
        bool hasSizeGrip( void ) const
        { return (bool)size_grip_; }
        
        //! size grip
        NitrogenSizeGrip& sizeGrip( void ) const
        { return *size_grip_; }
        
        //@}
        
        //! configuration
        NitrogenConfiguration configuration_;
        
        //! used to invalidate color cache
        bool colorCacheInvalid_;
        
        //! stored color
        QColor cachedTitlebarTextColor_;
        
        //! size grip widget
        NitrogenSizeGrip* size_grip_;

        #if KDE_IS_VERSION(4,2,92)
        ShadowTilesOption shadowTilesOption_;        
        ShadowTilesOption glowTilesOption_;
        TileSet *shadowTiles_;        
        TileSet *glowTiles_;
        #endif

        //! helper
        NitrogenHelper& helper_;    
        
        //! true when initialized
        bool initialized_;
        
    };
    
    
} // namespace Nitrogen

#endif // EXAMPLECLIENT_H
