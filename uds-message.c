#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include "gpio-monitor.h"

static void uds_stream_message( monitor_node *node )
{
    int s = socket( AF_UNIX, SOCK_STREAM, 0 );
    if ( s == -1 ) {
        perror( "socket() error in uds_stream_message()" );
        return;
    }

    struct sockaddr_un addr;
    memset( &addr, 0, sizeof( struct sockaddr_un ) );

    addr.sun_family = AF_UNIX;
    strncpy( addr.sun_path, node->socket_pathname, sizeof( addr.sun_path ) - 1 );

    int ret = connect( s, (const struct sockaddr *)&addr, sizeof( struct sockaddr_un ) );
    if ( ret == -1 ) {
        fprintf( stderr, "connect() error for addr '%s': %s.\n", node->socket_pathname, strerror( errno ) );
        close( s );
        return;
    }

    ret = write( s, node->socket_message, strlen( node->socket_message ) + 1 );
    if ( ret == -1 ) {
        perror( "write() error in uds_stream_message()" );
    }

    close( s );
}

static void uds_dgram_message( monitor_node *node )
{
    int s = socket( AF_UNIX, SOCK_DGRAM, 0 );
    if ( s == -1 ) {
        perror( "socket() error in uds_dgram_message()" );
        return;
    }

    struct sockaddr_un addr;
    memset( &addr, 0, sizeof( struct sockaddr_un ) );

    addr.sun_family = AF_UNIX;
    strncpy( addr.sun_path, node->socket_pathname, sizeof( addr.sun_path ) - 1 );

    if ( sendto( s, node->socket_message, strlen( node->socket_message ) + 1, 0,
                        (const struct sockaddr *)&addr, sizeof(struct sockaddr_un) ) < 0 ) {
        fprintf( stderr, "sendto() error for addr '%s': %s.\n", node->socket_pathname, strerror( errno ) );
    }

    close( s );
}

// gpio_value unused for now
void uds_message( monitor_node *node, int gpio_value )
{
    int pid = fork();
    if ( pid == -1 ) {
        perror( "fork error in uds_message" );
    }
    else if ( pid == 0 )
    {
        if ( node->socket_type == STREAM ) {
            uds_stream_message( node );
        }
        else if ( node->socket_type == DGRAM ) {
            uds_dgram_message( node );
        }

        exit(0);
    }
}
