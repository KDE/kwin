/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

Based on glcompmgr code by Felix Bellaby.
Using code from Compiz and Beryl.

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

// This file is included in scene_opengl.cpp

// the configs used for the destination
GLXFBConfig SceneOpenGL::fbcbuffer_db;
GLXFBConfig SceneOpenGL::fbcbuffer_nondb;
// the configs used for windows
SceneOpenGL::FBConfigInfo SceneOpenGL::fbcdrawableinfo[ 32 + 1 ];
// GLX content
GLXContext SceneOpenGL::ctxbuffer;
GLXContext SceneOpenGL::ctxdrawable;
// the destination drawable where the compositing is done
GLXDrawable SceneOpenGL::glxbuffer = None;
GLXDrawable SceneOpenGL::last_pixmap = None;

SceneOpenGL::SceneOpenGL( Workspace* ws )
    : Scene( ws )
    , init_ok( false )
    , selfCheckDone( false )
    , m_sceneShader( NULL )
    , m_genericSceneShader( NULL )
    {
    if( !Extensions::glxAvailable())
        {
        kDebug( 1212 ) << "No glx extensions available";
        return; // error
        }
    initGLX();
    // check for FBConfig support
    if( !hasGLExtension( "GLX_SGIX_fbconfig" ) || !glXGetFBConfigAttrib || !glXGetFBConfigs ||
            !glXGetVisualFromFBConfig || !glXCreatePixmap || !glXDestroyPixmap ||
            !glXCreateWindow || !glXDestroyWindow )
        {
        kError( 1212 ) << "GLX_SGIX_fbconfig or required GLX functions missing";
        return; // error
        }
    if( !selectMode())
        return; // error
    if( !initBuffer()) // create destination buffer
        return; // error
    if( !initRenderingContext())
        return; // error
    // Initialize OpenGL
    initGL();
    if( QString((const char*)glGetString( GL_RENDERER )) == "Software Rasterizer" )
        {
        kError( 1212 ) << "OpenGL Software Rasterizer detected. Falling back to XRender.";
        QTimer::singleShot( 0, Workspace::self(), SLOT( fallbackToXRenderCompositing()));
        return;
        }
    if( !hasGLExtension( "GL_ARB_texture_non_power_of_two" )
        && !hasGLExtension( "GL_ARB_texture_rectangle" ))
        {
        kError( 1212 ) << "GL_ARB_texture_non_power_of_two and GL_ARB_texture_rectangle missing";
        return; // error
        }
    if( db )
        glDrawBuffer( GL_BACK );
    // Check whether certain features are supported
    has_waitSync = false;
    if( glXGetVideoSync && glXIsDirect( display(), ctxbuffer ) && options->glVSync )
        {
        unsigned int sync;
        if( glXGetVideoSync( &sync ) == 0 )
            {
            if( glXWaitVideoSync( 1, 0, &sync ) == 0 )
                has_waitSync = true;
            else
                qWarning() << "NO VSYNC! glXWaitVideoSync(1,0,&uint) isn't 0 but" << glXWaitVideoSync( 1, 0, &sync );
            }
        else
            qWarning() << "NO VSYNC! glXGetVideoSync(&uint) isn't 0 but" << glXGetVideoSync( &sync );
        }

    debug = qstrcmp( qgetenv( "KWIN_GL_DEBUG" ), "1" ) == 0;

    // scene shader setup
    GLPlatform::instance()->detect();
    if( GLPlatform::instance()->supports( GLSL ) )
        {
        setupSceneShaders();
        }

    // OpenGL scene setup
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    float fovy = 60.0f;
    float aspect = 1.0f;
    float zNear = 0.1f;
    float zFar = 100.0f;
    float ymax = zNear * tan( fovy  * M_PI / 360.0f );
    float ymin = -ymax;
    float xmin =  ymin * aspect;
    float xmax = ymax * aspect;
    // swap top and bottom to have OpenGL coordinate system match X system
    glFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    float scaleFactor = 1.1 * tan( fovy * M_PI / 360.0f )/ymax;
    glTranslatef( xmin*scaleFactor, ymax*scaleFactor, -1.1 );
    glScalef( (xmax-xmin)*scaleFactor/displayWidth(), -(ymax-ymin)*scaleFactor/displayHeight(), 0.001 );
    if( checkGLError( "Init" ))
        {
        kError( 1212 ) << "OpenGL compositing setup failed";
        return; // error
        }
// selfcheck is broken (see Bug 253357)
#if 0
    // Do self-check immediatelly during compositing setup only when it's not KWin startup
    // at the same time (in other words, only when activating compositing using the kcm).
    // Currently selfcheck causes bad flicker (due to X mapping the overlay window
    // for too long?) which looks bad during KDE startup.
    if( !initting )
        {
        if( !selfCheck())
            return;
        selfCheckDone = true;
        }
#endif
    kDebug( 1212 ) << "DB:" << db << ", TFP:" << tfp_mode << ", SHM:" << shm_mode
        << ", Direct:" << bool( glXIsDirect( display(), ctxbuffer )) << endl;
    init_ok = true;
    }

SceneOpenGL::~SceneOpenGL()
    {
    if( !init_ok )
        {
        // TODO this probably needs to clean up whatever has been created until the failure
        wspace->destroyOverlay();
        return;
        }
    foreach( Window* w, windows )
        delete w;
    // do cleanup after initBuffer()
    glXMakeCurrent( display(), None, NULL );
    glXDestroyContext( display(), ctxbuffer );
    if( wspace->overlayWindow())
        {
        if( hasGLXVersion( 1, 3 ))
            glXDestroyWindow( display(), glxbuffer );
        XDestroyWindow( display(), buffer );
        wspace->destroyOverlay();
        }
    else
        {
        glXDestroyPixmap( display(), glxbuffer );
        XFreeGC( display(), gcroot );
        XFreePixmap( display(), buffer );
        }
    if( shm_mode )
        cleanupShm();
    if( !tfp_mode && !shm_mode )
        {
        if( last_pixmap != None )
            glXDestroyPixmap( display(), last_pixmap );
        glXDestroyContext( display(), ctxdrawable );
        }
    delete m_sceneShader;
    SceneOpenGL::EffectFrame::cleanup();
    checkGLError( "Cleanup" );
    }

bool SceneOpenGL::initTfp()
    {
    if( glXBindTexImageEXT == NULL || glXReleaseTexImageEXT == NULL )
        return false;
    return true;
    }

bool SceneOpenGL::initRenderingContext()
    {
    bool direct_rendering = options->glDirect;
    if( !tfp_mode && !shm_mode )
        direct_rendering = false; // fallback doesn't seem to work with direct rendering
    KXErrorHandler errs1;
    ctxbuffer = glXCreateNewContext( display(), fbcbuffer, GLX_RGBA_TYPE, NULL,
        direct_rendering ? GL_TRUE : GL_FALSE );
    bool failed = ( ctxbuffer == NULL || !glXMakeCurrent( display(), glxbuffer, ctxbuffer ));
    if( errs1.error( true )) // always check for error( having it all in one if() could skip
        failed = true;       // it due to evaluation short-circuiting
    if( failed )
        {
        if( !direct_rendering )
            {
            kDebug( 1212 ).nospace() << "Couldn't initialize rendering context ("
                << KXErrorHandler::errorMessage( errs1.errorEvent()) << ")";
            return false;
            }
    glXMakeCurrent( display(), None, NULL );
        if( ctxbuffer != NULL )
            glXDestroyContext( display(), ctxbuffer );
        direct_rendering = false; // try again
        KXErrorHandler errs2;
        ctxbuffer = glXCreateNewContext( display(), fbcbuffer, GLX_RGBA_TYPE, NULL, GL_FALSE );
        bool failed = ( ctxbuffer == NULL || !glXMakeCurrent( display(), glxbuffer, ctxbuffer ));
        if( errs2.error( true ))
            failed = true;
        if( failed )
            {
            kDebug( 1212 ).nospace() << "Couldn't initialize rendering context ("
                << KXErrorHandler::errorMessage( errs2.errorEvent()) << ")";
            return false;
            }
        }
    if( !tfp_mode && !shm_mode )
        {
        ctxdrawable = glXCreateNewContext( display(), fbcdrawableinfo[ QX11Info::appDepth() ].fbconfig, GLX_RGBA_TYPE, ctxbuffer,
            direct_rendering ? GL_TRUE : GL_FALSE );
        }
    return true;
    }

// create destination buffer
bool SceneOpenGL::initBuffer()
    {
    if( !initBufferConfigs())
        return false;
    if( fbcbuffer_db != NULL && wspace->createOverlay())
        { // we have overlay, try to create double-buffered window in it
        fbcbuffer = fbcbuffer_db;
        XVisualInfo* visual = glXGetVisualFromFBConfig( display(), fbcbuffer );
        XSetWindowAttributes attrs;
        attrs.colormap = XCreateColormap( display(), rootWindow(), visual->visual, AllocNone );
        buffer = XCreateWindow( display(), wspace->overlayWindow(), 0, 0, displayWidth(), displayHeight(),
            0, visual->depth, InputOutput, visual->visual, CWColormap, &attrs );
        if( hasGLXVersion( 1, 3 ))
            glxbuffer = glXCreateWindow( display(), fbcbuffer, buffer, NULL );
        else
            glxbuffer = buffer;
        wspace->setupOverlay( buffer );
        db = true;
        XFree( visual );
        }
    else if( fbcbuffer_nondb != NULL )
        { // cannot get any double-buffered drawable, will double-buffer using a pixmap
        fbcbuffer = fbcbuffer_nondb;
        XVisualInfo* visual = glXGetVisualFromFBConfig( display(), fbcbuffer );
        XGCValues gcattr;
        gcattr.subwindow_mode = IncludeInferiors;
        gcroot = XCreateGC( display(), rootWindow(), GCSubwindowMode, &gcattr );
        buffer = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(),
            visual->depth );
        glxbuffer = glXCreatePixmap( display(), fbcbuffer, buffer, NULL );
        db = false;
        XFree( visual );
        }
    else
        {
        kError( 1212 ) << "Couldn't create output buffer (failed to create overlay window?) !";
        return false; // error
        }
    int vis_buffer;
    glXGetFBConfigAttrib( display(), fbcbuffer, GLX_VISUAL_ID, &vis_buffer );
    XVisualInfo* visinfo_buffer = glXGetVisualFromFBConfig( display(), fbcbuffer );
    kDebug( 1212 ) << "Buffer visual (depth " << visinfo_buffer->depth << "): 0x" << QString::number( vis_buffer, 16 );
    XFree( visinfo_buffer );
    return true;
    }

// choose the best configs for the destination buffer
bool SceneOpenGL::initBufferConfigs()
    {
    int cnt;
    GLXFBConfig *fbconfigs = glXGetFBConfigs( display(), DefaultScreen( display() ), &cnt );
    fbcbuffer_db = NULL;
    fbcbuffer_nondb = NULL;

    for( int i = 0; i < 2; i++ )
        {
        int back, stencil, depth, caveat, alpha;
        back = i > 0 ? INT_MAX : 1;
        stencil = INT_MAX;
        depth = INT_MAX;
        caveat = INT_MAX;
        alpha = 0;
        for( int j = 0; j < cnt; j++ )
            {
            XVisualInfo *vi;
            int visual_depth;
            vi = glXGetVisualFromFBConfig( display(), fbconfigs[ j ] );
            if( vi == NULL )
                continue;
            visual_depth = vi->depth;
            XFree( vi );
            if( visual_depth != DefaultDepth( display(), DefaultScreen( display())))
                continue;
            int value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_ALPHA_SIZE, &alpha );
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_BUFFER_SIZE, &value );
            if( value != visual_depth && ( value - alpha ) != visual_depth )
                continue;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_RENDER_TYPE, &value );
            if( !( value & GLX_RGBA_BIT ))
                continue;
            int back_value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_DOUBLEBUFFER, &back_value );
            if( i > 0 )
                {
                if( back_value > back )
                    continue;
                }
            else
                {
                if( back_value < back )
                    continue;
                }
            int stencil_value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_STENCIL_SIZE, &stencil_value );
            if( stencil_value > stencil )
                continue;
            int depth_value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_DEPTH_SIZE, &depth_value );
            if( depth_value > depth )
                continue;
            int caveat_value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_CONFIG_CAVEAT, &caveat_value );
            if( caveat_value > caveat )
                continue;
            back = back_value;
            stencil = stencil_value;
            depth = depth_value;
            caveat = caveat_value;
            if( i > 0 )
                fbcbuffer_nondb = fbconfigs[ j ];
            else
                fbcbuffer_db = fbconfigs[ j ];
            }
        }
    if( cnt )
        XFree( fbconfigs );
    if( fbcbuffer_db == NULL && fbcbuffer_nondb == NULL )
        {
        kError( 1212 ) << "Couldn't find framebuffer configuration for buffer!";
        return false;
        }
    for( int i = 0; i <= 32; i++ )
        {
        if( fbcdrawableinfo[ i ].fbconfig == NULL )
            continue;
        int vis_drawable = 0;
        glXGetFBConfigAttrib( display(), fbcdrawableinfo[ i ].fbconfig, GLX_VISUAL_ID, &vis_drawable );
        kDebug( 1212 ) << "Drawable visual (depth " << i << "): 0x" << QString::number( vis_drawable, 16 );
        }
    return true;
    }

// make a list of the best configs for windows by depth
bool SceneOpenGL::initDrawableConfigs()
    {
    int cnt;
    GLXFBConfig *fbconfigs = glXGetFBConfigs( display(), DefaultScreen( display() ), &cnt );

    for( int i = 0; i <= 32; i++ )
        {
        int back, stencil, depth, caveat, alpha, mipmap, rgba;
        back = INT_MAX;
        stencil = INT_MAX;
        depth = INT_MAX;
        caveat = INT_MAX;
        mipmap = 0;
        rgba = 0;
        fbcdrawableinfo[ i ].fbconfig = NULL;
        fbcdrawableinfo[ i ].bind_texture_format = 0;
        fbcdrawableinfo[ i ].texture_targets = 0;
        fbcdrawableinfo[ i ].y_inverted = 0;
        fbcdrawableinfo[ i ].mipmap = 0;
        for( int j = 0; j < cnt; j++ )
            {
            XVisualInfo *vi;
            int visual_depth;
            vi = glXGetVisualFromFBConfig( display(), fbconfigs[ j ] );
            if( vi == NULL )
                continue;
            visual_depth = vi->depth;
            XFree( vi );
            if( visual_depth != i )
                continue;
            int value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_ALPHA_SIZE, &alpha );
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_BUFFER_SIZE, &value );
            if( value != i && ( value - alpha ) != i )
                continue;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_RENDER_TYPE, &value );
            if( !( value & GLX_RGBA_BIT ))
                continue;
            if( tfp_mode )
                {
                value = 0;
                if( i == 32 )
                    {
                    glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                          GLX_BIND_TO_TEXTURE_RGBA_EXT, &value );
                    if( value )
                        {
                        // TODO I think this should be set only after the config passes all tests
                        rgba = 1;
                        fbcdrawableinfo[ i ].bind_texture_format = GLX_TEXTURE_FORMAT_RGBA_EXT;
                        }
                    }
                if( !value )
                    {
                    if( rgba )
                        continue;
                    glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                          GLX_BIND_TO_TEXTURE_RGB_EXT, &value );
                    if( !value )
                        continue;
                    fbcdrawableinfo[ i ].bind_texture_format = GLX_TEXTURE_FORMAT_RGB_EXT;
                    }
                }
            int back_value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_DOUBLEBUFFER, &back_value );
            if( back_value > back )
                continue;
            int stencil_value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_STENCIL_SIZE, &stencil_value );
            if( stencil_value > stencil )
                continue;
            int depth_value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_DEPTH_SIZE, &depth_value );
            if( depth_value > depth )
                continue;
            int mipmap_value = -1;
            if( tfp_mode && GLTexture::framebufferObjectSupported())
                {
                glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                      GLX_BIND_TO_MIPMAP_TEXTURE_EXT, &mipmap_value );
                if( mipmap_value < mipmap )
                    continue;
                }
            int caveat_value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_CONFIG_CAVEAT, &caveat_value );
            if( caveat_value > caveat )
                continue;
            // ok, config passed all tests, it's the best one so far
            fbcdrawableinfo[ i ].fbconfig = fbconfigs[ j ];
            caveat = caveat_value;
            back = back_value;
            stencil = stencil_value;
            depth = depth_value;
            mipmap = mipmap_value;
            if ( tfp_mode )
                {
                glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                      GLX_BIND_TO_TEXTURE_TARGETS_EXT, &value );
                fbcdrawableinfo[ i ].texture_targets = value;
                }
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_Y_INVERTED_EXT, &value );
            fbcdrawableinfo[ i ].y_inverted = value;
            fbcdrawableinfo[ i ].mipmap = mipmap;
            }
        }
    if( cnt )
        XFree( fbconfigs );
    if( fbcdrawableinfo[ DefaultDepth( display(), DefaultScreen( display())) ].fbconfig == NULL )
        {
        kError( 1212 ) << "Couldn't find framebuffer configuration for default depth!";
        return false;
        }
    if( fbcdrawableinfo[ 32 ].fbconfig == NULL )
        {
        kError( 1212 ) << "Couldn't find framebuffer configuration for depth 32 (no ARGB GLX visual)!";
        return false;
        }
    return true;
    }

void SceneOpenGL::selfCheckSetup()
    {
    KXErrorHandler err;
    QImage img( selfCheckWidth(), selfCheckHeight(), QImage::Format_RGB32 );
    img.setPixel( 0, 0, QColor( Qt::red ).rgb());
    img.setPixel( 1, 0, QColor( Qt::green ).rgb());
    img.setPixel( 2, 0, QColor( Qt::blue ).rgb());
    img.setPixel( 0, 1, QColor( Qt::white ).rgb());
    img.setPixel( 1, 1, QColor( Qt::black ).rgb());
    img.setPixel( 2, 1, QColor( Qt::white ).rgb());
    QPixmap pix = QPixmap::fromImage( img );
    foreach( const QPoint& p, selfCheckPoints())
        {
        XSetWindowAttributes wa;
        wa.override_redirect = True;
        ::Window window = XCreateWindow( display(), rootWindow(), 0, 0, selfCheckWidth(), selfCheckHeight(),
            0, QX11Info::appDepth(), CopyFromParent, CopyFromParent, CWOverrideRedirect, &wa );
        XSetWindowBackgroundPixmap( display(), window, pix.handle());
        XClearWindow( display(), window );
        XMapWindow( display(), window );
        // move the window one down to where the result will be rendered too, just in case
        // the render would fail completely and eventual check would try to read this window's contents
        XMoveWindow( display(), window, p.x() + 1, p.y());
        XCompositeRedirectWindow( display(), window, CompositeRedirectAutomatic );
        Pixmap wpix = XCompositeNameWindowPixmap( display(), window );
        glXWaitX();
        Texture texture;
        texture.load( wpix, QSize( selfCheckWidth(), selfCheckHeight()), QX11Info::appDepth());
        texture.bind();
        QRect rect( p.x(), p.y(), selfCheckWidth(), selfCheckHeight());
        texture.render( infiniteRegion(), rect );
        texture.unbind();
        glXWaitGL();
        XFreePixmap( display(), wpix );
        XDestroyWindow( display(), window );
        }
    err.error( true ); // just sync and discard
    }

bool SceneOpenGL::selfCheckFinish()
    {
    glXWaitGL();
    KXErrorHandler err;
    bool ok = true;
    foreach( const QPoint& p, selfCheckPoints())
        {
        QPixmap pix = QPixmap::grabWindow( rootWindow(), p.x(), p.y(), selfCheckWidth(), selfCheckHeight());
        QImage img = pix.toImage();
//        kDebug(1212) << "P:" << QColor( img.pixel( 0, 0 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 1, 0 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 2, 0 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 0, 1 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 1, 1 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 2, 1 )).name();
        if( img.pixel( 0, 0 ) != QColor( Qt::red ).rgb()
            || img.pixel( 1, 0 ) != QColor( Qt::green ).rgb()
            || img.pixel( 2, 0 ) != QColor( Qt::blue ).rgb()
            || img.pixel( 0, 1 ) != QColor( Qt::white ).rgb()
            || img.pixel( 1, 1 ) != QColor( Qt::black ).rgb()
            || img.pixel( 2, 1 ) != QColor( Qt::white ).rgb())
            {
            kError( 1212 ) << "OpenGL compositing self-check failed, disabling compositing.";
            ok = false;
            break;
            }
        }
    if( err.error( true ))
        ok = false;
    if( ok )
        kDebug( 1212 ) << "OpenGL compositing self-check passed.";
    if( !ok && options->disableCompositingChecks )
        {
        kWarning( 1212 ) << "Compositing checks disabled, proceeding regardless of self-check failure.";
        return true;
        }
    return ok;
    }

// the entry function for painting
void SceneOpenGL::paint( QRegion damage, ToplevelList toplevels )
    {
    QTime t = QTime::currentTime();
    foreach( Toplevel* c, toplevels )
        {
        assert( windows.contains( c ));
        stacking_order.append( windows[ c ] );
        }
    grabXServer();
    glXWaitX();
    glPushMatrix();
    int mask = 0;
#ifdef CHECK_GL_ERROR
    checkGLError( "Paint1" );
#endif
    paintScreen( &mask, &damage ); // call generic implementation
#ifdef CHECK_GL_ERROR
    checkGLError( "Paint2" );
#endif
    glPopMatrix();
    ungrabXServer(); // ungrab before flushBuffer(), it may wait for vsync
    if( wspace->overlayWindow()) // show the window only after the first pass, since
        wspace->showOverlay();   // that pass may take long
// selfcheck is broken (see Bug 253357)
#if 0
    if( !selfCheckDone )
        {
        selfCheckSetup();
        damage |= selfCheckRegion();
        }
#endif
    lastRenderTime = t.elapsed();
    flushBuffer( mask, damage );
#if 0
    if( !selfCheckDone )
        {
        if( !selfCheckFinish())
            QTimer::singleShot( 0, Workspace::self(), SLOT( finishCompositing()));
        selfCheckDone = true;
        }
#endif
    // do cleanup
    stacking_order.clear();
    checkGLError( "PostPaint" );
    }

// wait for vblank signal before painting
void SceneOpenGL::waitSync()
    { // NOTE that vsync has no effect with indirect rendering
    if( waitSyncAvailable())
        {
        uint sync;
        glFlush();
        glXGetVideoSync( &sync );
        glXWaitVideoSync( 2, ( sync + 1 ) % 2, &sync );
        }
    }

// actually paint to the screen (double-buffer swap or copy from pixmap buffer)
void SceneOpenGL::flushBuffer( int mask, QRegion damage )
    {
    if( db )
        {
        if( mask & PAINT_SCREEN_REGION )
            {
            waitSync();
            if( glXCopySubBuffer )
                {
                foreach( const QRect &r, damage.rects())
                    {
                    // convert to OpenGL coordinates
                    int y = displayHeight() - r.y() - r.height();
                    glXCopySubBuffer( display(), glxbuffer, r.x(), y, r.width(), r.height());
                    }
                }
            else
                { // no idea why glScissor() is used, but Compiz has it and it doesn't seem to hurt
                glEnable( GL_SCISSOR_TEST );
                glDrawBuffer( GL_FRONT );
                int xpos = 0;
                int ypos = 0;
                foreach( const QRect &r, damage.rects())
                    {
                    // convert to OpenGL coordinates
                    int y = displayHeight() - r.y() - r.height();
                    // Move raster position relatively using glBitmap() rather
                    // than using glRasterPos2f() - the latter causes drawing
                    // artefacts at the bottom screen edge with some gfx cards
//                    glRasterPos2f( r.x(), r.y() + r.height());
                    glBitmap( 0, 0, 0, 0, r.x() - xpos, y - ypos, NULL );
                    xpos = r.x();
                    ypos = y;
                    glScissor( r.x(), y, r.width(), r.height());
                    glCopyPixels( r.x(), y, r.width(), r.height(), GL_COLOR );
                    }
                glBitmap( 0, 0, 0, 0, -xpos, -ypos, NULL ); // move position back to 0,0
                glDrawBuffer( GL_BACK );
                glDisable( GL_SCISSOR_TEST );
                }
            }
        else
            {
            waitSync();
            glXSwapBuffers( display(), glxbuffer );
            }
        glXWaitGL();
        XFlush( display());
        }
    else
        {
        glFlush();
        glXWaitGL();
        waitSync();
        if( mask & PAINT_SCREEN_REGION )
            foreach( const QRect &r, damage.rects())
                XCopyArea( display(), buffer, rootWindow(), gcroot, r.x(), r.y(), r.width(), r.height(), r.x(), r.y());
        else
            XCopyArea( display(), buffer, rootWindow(), gcroot, 0, 0, displayWidth(), displayHeight(), 0, 0 );
        XFlush( display());
        }
    }

void SceneOpenGL::paintGenericScreen( int mask, ScreenPaintData data )
    {
    if( mask & PAINT_SCREEN_TRANSFORMED )
        { // apply screen transformations
        glPushMatrix();
        glTranslatef( data.xTranslate, data.yTranslate, data.zTranslate );
        if( data.rotation )
            {
            // translate to rotation point, rotate, translate back
            glTranslatef( data.rotation->xRotationPoint, data.rotation->yRotationPoint, data.rotation->zRotationPoint );
            float xAxis = 0.0;
            float yAxis = 0.0;
            float zAxis = 0.0;
            switch( data.rotation->axis )
                {
                case RotationData::XAxis:
                    xAxis = 1.0;
                    break;
                case RotationData::YAxis:
                    yAxis = 1.0;
                    break;
                case RotationData::ZAxis:
                    zAxis = 1.0;
                    break;
                }
            glRotatef( data.rotation->angle, xAxis, yAxis, zAxis );
            glTranslatef( -data.rotation->xRotationPoint, -data.rotation->yRotationPoint, -data.rotation->zRotationPoint );
            }
        glScalef( data.xScale, data.yScale, data.zScale );
        }
    Scene::paintGenericScreen( mask, data );
    if( mask & PAINT_SCREEN_TRANSFORMED )
        {
        glPopMatrix();
        }
    }

void SceneOpenGL::paintBackground( QRegion region )
    {
    PaintClipper pc( region );
    if( !PaintClipper::clip())
        {
        glPushAttrib( GL_COLOR_BUFFER_BIT );
        glClearColor( 0, 0, 0, 1 ); // black
        glClear( GL_COLOR_BUFFER_BIT );
        glPopAttrib();
        return;
        }
    if( pc.clip() && pc.paintArea().isEmpty())
        return; // no background to paint
    glPushAttrib( GL_CURRENT_BIT );
    glColor4f( 0, 0, 0, 1 ); // black
    for( PaintClipper::Iterator iterator;
         !iterator.isDone();
         iterator.next())
        {
        glBegin( GL_QUADS );
        QRect r = iterator.boundingRect();
        glVertex2i( r.x(), r.y());
        glVertex2i( r.x() + r.width(), r.y());
        glVertex2i( r.x() + r.width(), r.y() + r.height());
        glVertex2i( r.x(), r.y() + r.height());
        glEnd();
        }
    glPopAttrib();
    }

//****************************************
// SceneOpenGL::Texture
//****************************************

void SceneOpenGL::Texture::init()
    {
    glxpixmap = None;
    }

void SceneOpenGL::Texture::release()
    {
    if( tfp_mode && glxpixmap != None )
        {
        glXReleaseTexImageEXT( display(), glxpixmap, GLX_FRONT_LEFT_EXT );
        glXDestroyPixmap( display(), glxpixmap );
        glxpixmap = None;
        }
    }

void SceneOpenGL::Texture::findTarget()
    {
    unsigned int new_target = 0;
    if( tfp_mode && glXQueryDrawable && glxpixmap != None )
        glXQueryDrawable( display(), glxpixmap, GLX_TEXTURE_TARGET_EXT, &new_target );
    // Hack for XGL - this should not be a fallback for glXQueryDrawable() but instead the case
    // when glXQueryDrawable is not available. However this call fails with XGL, unless KWin
    // is compiled statically with the libGL that Compiz is built against (without which neither
    // Compiz works with XGL). Falling back to doing this manually makes this work.
    if( new_target == 0 )
        {
        if( NPOTTextureSupported() ||
            ( isPowerOfTwo( mSize.width()) && isPowerOfTwo( mSize.height())))
            new_target = GLX_TEXTURE_2D_EXT;
        else
            new_target = GLX_TEXTURE_RECTANGLE_EXT;
        }
    switch( new_target )
        {
        case GLX_TEXTURE_2D_EXT:
            mTarget = GL_TEXTURE_2D;
            mScale.setWidth( 1.0f / mSize.width());
            mScale.setHeight( 1.0f / mSize.height());
          break;
        case GLX_TEXTURE_RECTANGLE_EXT:
            mTarget = GL_TEXTURE_RECTANGLE_ARB;
            mScale.setWidth( 1.0f );
            mScale.setHeight( 1.0f );
          break;
        default:
            abort();
        }
    }

bool SceneOpenGL::Texture::load( const Pixmap& pix, const QSize& size,
    int depth, QRegion region )
    {
#ifdef CHECK_GL_ERROR
    checkGLError( "TextureLoad1" );
#endif
    if( pix == None || size.isEmpty() || depth < 1 )
        return false;
    if( tfp_mode )
        {
        if( fbcdrawableinfo[ depth ].fbconfig == NULL )
            {
            kDebug( 1212 ) << "No framebuffer configuration for depth " << depth
                           << "; not binding pixmap" << endl;
            return false;
            }
        }

    mSize = size;
    if( mTexture == None || !region.isEmpty())
        { // new texture, or texture contents changed; mipmaps now invalid
        setDirty();
        }

#ifdef CHECK_GL_ERROR
    checkGLError( "TextureLoad2" );
#endif
    if( tfp_mode )
        { // tfp mode, simply bind the pixmap to texture
        if( mTexture == None )
            createTexture();
        // The GLX pixmap references the contents of the original pixmap, so it doesn't
        // need to be recreated when the contents change.
        // The texture may or may not use the same storage depending on the EXT_tfp
        // implementation. When options->glStrictBinding is true, the texture uses
        // a different storage and needs to be updated with a call to
        // glXBindTexImageEXT() when the contents of the pixmap has changed.
        if( glxpixmap != None )
            glBindTexture( mTarget, mTexture );
        else
            {
            int attrs[] =
                {
                GLX_TEXTURE_FORMAT_EXT, fbcdrawableinfo[ depth ].bind_texture_format,
                GLX_MIPMAP_TEXTURE_EXT, fbcdrawableinfo[ depth ].mipmap,
                None, None, None
                };
            if ( ( fbcdrawableinfo[ depth ].texture_targets & GLX_TEXTURE_2D_BIT_EXT ) &&
                 ( GLTexture::NPOTTextureSupported() ||
                   ( isPowerOfTwo(size.width()) && isPowerOfTwo(size.height()) )))
                {
                attrs[ 4 ] = GLX_TEXTURE_TARGET_EXT;
                attrs[ 5 ] = GLX_TEXTURE_2D_EXT;
                }
            else if ( fbcdrawableinfo[ depth ].texture_targets & GLX_TEXTURE_RECTANGLE_BIT_EXT )
                {
                attrs[ 4 ] = GLX_TEXTURE_TARGET_EXT;
                attrs[ 5 ] = GLX_TEXTURE_RECTANGLE_EXT;
                }
            glxpixmap = glXCreatePixmap( display(), fbcdrawableinfo[ depth ].fbconfig, pix, attrs );
#ifdef CHECK_GL_ERROR
            checkGLError( "TextureLoadTFP1" );
#endif
            findTarget();
            y_inverted = fbcdrawableinfo[ depth ].y_inverted ? true : false;
            can_use_mipmaps = fbcdrawableinfo[ depth ].mipmap ? true : false;
            glBindTexture( mTarget, mTexture );
#ifdef CHECK_GL_ERROR
            checkGLError( "TextureLoadTFP2" );
#endif
            if( !options->glStrictBinding )
                glXBindTexImageEXT( display(), glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
            }
        if( options->glStrictBinding )
            // Mark the texture as damaged so it will be updated on the next call to bind()
            damaged = true;
        }
    else if( shm_mode )
        { // copy pixmap contents to a texture via shared memory
#ifdef HAVE_XSHM
        GLenum pixfmt, type;
        if( depth >= 24 )
            {
            pixfmt = GL_BGRA;
            type = GL_UNSIGNED_BYTE;
            }
        else
            { // depth 16
            pixfmt = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
            }
        findTarget();
#ifdef CHECK_GL_ERROR
        checkGLError( "TextureLoadSHM1" );
#endif
        if( mTexture == None )
            {
            createTexture();
            glBindTexture( mTarget, mTexture );
            y_inverted = false;
            glTexImage2D( mTarget, 0, depth == 32 ? GL_RGBA : GL_RGB,
                mSize.width(), mSize.height(), 0,
                pixfmt, type, NULL );
            }
        else
            glBindTexture( mTarget, mTexture );
        if( !region.isEmpty())
            {
            XGCValues xgcv;
            xgcv.graphics_exposures = False;
            xgcv.subwindow_mode = IncludeInferiors;
            GC gc = XCreateGC( display(), pix, GCGraphicsExposures | GCSubwindowMode, &xgcv );
            Pixmap p = XShmCreatePixmap( display(), rootWindow(), shm.shmaddr, &shm,
                mSize.width(), mSize.height(), depth );
            QRegion damage = optimizeBindDamage( region, 100 * 100 );
            glPixelStorei( GL_UNPACK_ROW_LENGTH, mSize.width());
            foreach( const QRect &r, damage.rects())
                { // TODO for small areas it might be faster to not use SHM to avoid the XSync()
                XCopyArea( display(), pix, p, gc, r.x(), r.y(), r.width(), r.height(), 0, 0 );
                glXWaitX();
                glTexSubImage2D( mTarget, 0,
                    r.x(), r.y(), r.width(), r.height(),
                    pixfmt, type, shm.shmaddr );
                glXWaitGL();
                }
            glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
            XFreePixmap( display(), p );
            XFreeGC( display(), gc );
            }
#ifdef CHECK_GL_ERROR
        checkGLError( "TextureLoadSHM2" );
#endif
        y_inverted = true;
        can_use_mipmaps = true;
#endif
        }
    else
        { // fallback, copy pixmap contents to a texture
        // note that if depth is not QX11Info::appDepth(), this may
        // not work (however, it does seem to work with nvidia)
        findTarget();
        GLXDrawable pixmap = glXCreatePixmap( display(), fbcdrawableinfo[ QX11Info::appDepth() ].fbconfig, pix, NULL );
        glXMakeCurrent( display(), pixmap, ctxdrawable );
        if( last_pixmap != None )
            glXDestroyPixmap( display(), last_pixmap );
        // workaround for ATI - it leaks/crashes when the pixmap is destroyed immediately
        // here (http://lists.kde.org/?l=kwin&m=116353772208535&w=2)
        last_pixmap = pixmap;
        glReadBuffer( GL_FRONT );
        glDrawBuffer( GL_FRONT );
        if( mTexture == None )
            {
            createTexture();
            glBindTexture( mTarget, mTexture );
            y_inverted = false;
            glCopyTexImage2D( mTarget, 0,
                depth == 32 ? GL_RGBA : GL_RGB,
                0, 0, mSize.width(), mSize.height(), 0 );
            }
        else
            {
            glBindTexture( mTarget, mTexture );
            QRegion damage = optimizeBindDamage( region, 30 * 30 );
            foreach( const QRect &r, damage.rects())
                {
                // convert to OpenGL coordinates (this is mapping
                // the pixmap to a texture, this is not affected
                // by using glOrtho() for the OpenGL scene)
                int gly = mSize.height() - r.y() - r.height();
                glCopyTexSubImage2D( mTarget, 0,
                    r.x(), gly, r.x(), gly, r.width(), r.height());
                }
            }
        glXWaitGL();
        if( db )
            glDrawBuffer( GL_BACK );
        glXMakeCurrent( display(), glxbuffer, ctxbuffer );
        glBindTexture( mTarget, mTexture );
        y_inverted = false;
        can_use_mipmaps = true;
        }
#ifdef CHECK_GL_ERROR
    checkGLError( "TextureLoad0" );
#endif
    return true;
    }

void SceneOpenGL::Texture::bind()
    {
    glEnable( mTarget );
    glBindTexture( mTarget, mTexture );
    if( tfp_mode && options->glStrictBinding )
        {
        assert( glxpixmap != None );
        glXReleaseTexImageEXT( display(), glxpixmap, GLX_FRONT_LEFT_EXT );
        glXBindTexImageEXT( display(), glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
        setDirty(); // Mipmaps have to be regenerated after updating the texture
        }
    enableFilter();
    if( hasGLVersion( 1, 4, 0 ))
        {
        // Lod bias makes the trilinear-filtered texture look a bit sharper
        glTexEnvf( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -1.0f );
        }
    }

void SceneOpenGL::Texture::unbind()
    {
    if( hasGLVersion( 1, 4, 0 ))
        {
        glTexEnvf( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 0.0f );
        }
    if( tfp_mode && options->glStrictBinding )
        {
        assert( glxpixmap != None );
        glBindTexture( mTarget, mTexture );
        glXReleaseTexImageEXT( display(), glxpixmap, GLX_FRONT_LEFT_EXT );
        }

    GLTexture::unbind();
    }
