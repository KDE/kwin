/*

 LD_PRELOAD library that gives statistic on number of roundtrips in an application.

 $XREPLY_BACKTRACE defines whether and how backtraces will be printed for every
 roundtrip. If not set, only total number of roundtrips is printed after the process
 exits. If set to a number, backtrace for every roundtrip will be printed, and the
 backtraces will be as deep as the given number. If set to C<number> (e.g. C10),
 the backtraces will be "compressed" - every backtrace will be printed only once
 after the process exits, together with number of times it occurred.

*/

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <execinfo.h>
#include <assert.h>
#include <X11/Xlibint.h>

/* Since these symbols are weak, the apps can provide their own, and therefore
   e.g. temporarily suspend counting of roundtrips. At least theoretically,
   I haven't really tried it.
*/
__attribute((weak)) long ___xreply_reply_count = 0;
__attribute((weak)) int ___xreply_reply_enabled = 1;

#define MAX_BACKTRACES 1024

extern long ___xreply_reply_count;
extern int ___xreply_reply_enabled;

typedef Status (*xreply_ptr_t)(Display*,xReply*,int,Bool);

static xreply_ptr_t xreply_ptr = NULL;
static int xreply_backtrace_set = 0;
static int xreply_backtrace_type = 0;

struct xreply_struct
    {
    char* key;
    char* text;
    int count;
    };
static struct xreply_struct backtraces[ MAX_BACKTRACES ];
static int backtraces_size = 0;

static int xreply_compare( const void* left, const void* right )
    {
    int left_count = ((struct xreply_struct*)left)->count;
    int right_count = ((struct xreply_struct*)right)->count;
    return right_count - left_count;
    }

static void xreply_print(void)
    {
    char tmp[ 1024 ];
    int fd;
    fd = open( "/proc/self/cmdline", O_RDONLY );
    if( fd >= 0 )
        {
        read( fd, tmp, 1024 );
        tmp[ 1023 ] = '\0';
        close( fd );
        }
    fprintf( stderr, "XREPLY (%d : %s): %ld\n", getpid(), tmp, ___xreply_reply_count );
    if( xreply_backtrace_type < 0 )
        {
        int i;
        qsort( backtraces, backtraces_size, sizeof( struct xreply_struct ), xreply_compare );
        for( i = 0;
             i < backtraces_size;
             ++i )
            fprintf( stderr, "%d:%s\n\n", backtraces[ i ].count, backtraces[ i ].text );
        }
    }

static void xreply_backtrace()
    {
    void* trace[256];
    int n = backtrace(trace, 256);
    char** strings = backtrace_symbols (trace, n);

    if( xreply_backtrace_type > 0 )
        {
        fprintf( stderr, "%ld [\n", ___xreply_reply_count );
        if( n > xreply_backtrace_type )
            n = xreply_backtrace_type;
        int i;
        for( i = 0;
             i < n;
             ++i )
            fprintf( stderr, "%d: %s\n", i, strings[ i ] );
        fprintf( stderr, "]\n" );
        }
    else
        {
        char stack[ 256 * 20 ];
        int pos = 0;
        int i;
        stack[ 0 ] = '\0';
        if( n > -xreply_backtrace_type )
            n = -xreply_backtrace_type;
        for( i = 0;
             i < n;
             ++i )
            {
            const char* start = strrchr( strings[ i ], '[' );
            if( start == NULL )
                assert( !"No [ in address." );
            long addr;
            if( sscanf( start + 1, "0x%lx", &addr ) != 1 )
                assert( !"Failed to parse address." );
            if( sizeof( void* ) == 4 )
                {
                sprintf( stack + pos, "0x%8lx", addr );
                pos += 10;
                }
            else if( sizeof( void* ) == 8 )
                {
                sprintf( stack + pos, "0x%16lx", addr );
                pos += 18;
                }
            else
                assert( !"Unknown sizeof( void* )." );
            }
        for( i = 0;
             i < backtraces_size;
             ++i )
            if( strcmp( backtraces[ i ].key, stack ) == 0 )
                {
                ++backtraces[ i ].count;
                break;
                }
        if( i == backtraces_size )
            {
            int stack_text_size = 10;
            char* stack_text;
            char* stack_text_pos;
            for( i = 0;
                 i < n;
                 ++i )
                stack_text_size += strlen( strings[ i ] ) + 5;
            stack_text = stack_text_pos = malloc( stack_text_size );
            for( i = 0;
                 i < n;
                 ++i )
                {
                stack_text_pos = stpcpy( stack_text_pos, "\n" );
                stack_text_pos = stpcpy( stack_text_pos, strings[ i ] );
                }
            backtraces[ backtraces_size ].key = strdup( stack );
            backtraces[ backtraces_size ].text = stack_text;
            backtraces[ backtraces_size ].count = 1;
            ++backtraces_size;
            if( backtraces_size >= MAX_BACKTRACES )
                assert( !"MAX_BACKTRACES reached." );
            }
        }
    free (strings);
    }

Status
_XReply (dpy, rep, extra, discard)
    register Display *dpy;
    register xReply *rep;
    int extra;		/* number of 32-bit words expected after the reply */
    Bool discard;	/* should I discard data following "extra" words? */
    {
    if( ___xreply_reply_enabled )
        ++___xreply_reply_count;
    if( xreply_backtrace_set == 0 )
        {
        if( getenv( "XREPLY_BACKTRACE" ) != NULL )
            { // C<number> - compress backtraces, saved as negative value in xreply_backtrace_type
            if( getenv( "XREPLY_BACKTRACE" )[ 0 ] == 'C' )
                xreply_backtrace_type = -atoi( getenv( "XREPLY_BACKTRACE" ) + 1 );
            else // <number> - print the backtrace every time
                xreply_backtrace_type = atoi( getenv( "XREPLY_BACKTRACE" ));
            }
        else
            xreply_backtrace_type = 0;
        }
    if( xreply_backtrace_type != 0 )
        xreply_backtrace();
    if( xreply_ptr == NULL )
        {
        xreply_ptr = (xreply_ptr_t)dlsym( RTLD_NEXT, "_XReply" );
        if( xreply_ptr == NULL )
            assert( !"dlsym() failed." );
        atexit( xreply_print );
        }
    return xreply_ptr( dpy, rep, extra, discard );
    }
