/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Fredrik Höglund <fredrik@kde.org>

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

#include <X11/Xlib.h>
#include <GL/glx.h>
#include <string.h>
#include <stdio.h>


// Return 0 if we can use a direct context, 1 otherwise
int main(int argc, char *argv[])
{
    Display *dpy = XOpenDisplay(0);

    int error_base, event_base;
    if (!glXQueryExtension(dpy, &error_base, &event_base))
        return 1;

    int major, minor;
    if (!glXQueryVersion(dpy, &major, &minor))
        return 1;

    int attribs[] = {
        GLX_RGBA,
        GLX_RED_SIZE,    1,
        GLX_GREEN_SIZE,  1,
        GLX_BLUE_SIZE,   1,
        None,
        None
    };

    // Try to find an RGBA visual
    XVisualInfo *xvi = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
    if (!xvi) {
        // Try again for a doubled buffered visual
        attribs[sizeof(attribs) / sizeof(int) - 2] = GLX_DOUBLEBUFFER;
        xvi = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
    }

    if (!xvi)
        return 1;

    // Create a direct rendering context
    GLXContext ctx = glXCreateContext(dpy, xvi, NULL, True);
    if (!glXIsDirect(dpy, ctx))
        return 1;

    // Create a window using the visual.
    // We only need it to make the context current
    XSetWindowAttributes attr;
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(dpy, DefaultRootWindow(dpy), xvi->visual, AllocNone);
    Window win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0,
                               xvi->depth, InputOutput, xvi->visual,
                               CWBackPixel | CWBorderPixel | CWColormap, &attr);

    // Try to make the context current
    if (!glXMakeCurrent(dpy, win, ctx))
        return 1;

    // glXCreatePixmap() is a GLX 1.3+ function, but it's also provided by EXT_texture_from_pixmap
    const char *glxExtensions = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    if ((major == 1 && minor < 3) && !strstr(glxExtensions, "GLX_EXT_texture_from_pixmap"))
        return 1;

    // Assume that all Mesa drivers support direct rendering
    const GLubyte *version = glGetString(GL_VERSION);
    if (strstr((const char *)version, "Mesa"))
        return 0;

    // Direct contexts also work with the NVidia driver
    const GLubyte *vendor = glGetString(GL_VENDOR);
    if (strstr((const char *)vendor, "NVIDIA"))
        return 0;

    return 1;
}

