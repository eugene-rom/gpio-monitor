#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>

#include "gpio-monitor.h"

//  IN DEVELOPMENT
//     TEST CODE
// NON-FUNCTIONAL YET

int read_config( const char *filename );

int main( int argc, char *argv[] )
{
    // test reading config file
    if ( read_config( "gpio-monitor.conf" ) == -1 ) {
        return 1;
    }

    if ( argc < 2 ) {
        printf( "Usage:\n   gpio-monitor <number> [<number> ...]\n" );
        return 0;
    }

    nfds_t nfds = argc - 1;
    printf( "Monitoring numbers: " );
    int numbers[ nfds ];
    int i;
    for ( i = 0; i < nfds; i++ ) {
        char *end;
        numbers[ i ] = strtol( argv[ i + 1 ], &end, 10 );
        printf( ( i == 0 ) ? "%d" : ", %d", numbers[ i ] );
    }
    printf( ".\n" );

    printf( "fds: " );
    int fds[ nfds ];
    for ( i = 0; i < nfds; i++ )
    {
        fds[i] = gpio_open_value_file( numbers[i] );
        if ( fds[i] == -1 ) {
            return 0;
        }
        printf( ( i == 0 ) ? "%d" : ", %d", fds[ i ] );
    }
    printf( ".\n" );

    nfds_t j;
    struct pollfd *pfds = calloc( nfds, sizeof( struct pollfd ) );
    for ( j = 0; j < nfds; j++ ) {
        pfds[j].fd = fds[j];
        pfds[j].events = POLLPRI | POLLERR;
    }

    int counter = 0;

    while ( 1 )
    {
        int ready = poll( pfds, nfds, -1 );
        if ( ready == -1 ) {
            perror( "poll()" );
            break;
        }
        printf( "Ready: %d\n", ready );

        for ( j = 0; j < nfds; j++ )
        {
            if ( pfds[j].revents != 0 )
            {
                printf( "  #%d fd=%d; events: 0x%x\n", counter, pfds[j].fd, (int)pfds[j].revents );

                if ( pfds[j].revents & POLLPRI ) {
                    int val = gpio_read_value_file( pfds[j].fd );
                    printf( "#%d PRI Value: %d\n", counter, val );
                }
                else if ( pfds[j].revents & POLLERR ) {
                    int val = gpio_read_value_file( pfds[j].fd );
                    printf( "#%d ERR Value: %d\n", counter, val );
                }
            }
        }

        counter++;
    }
    return 0;
}

int line_is_empty_or_comment( const char *line )
{
    int i;
    int len = strlen( line );

    // empty?
    if ( ( len == 0 ) || ( line[ 0 ] == '\n' ) ) {
        return 1;
    }

    // skip spaces
    for ( i = 0; i < len; i++ ) {
        if ( ( line[ i ] != ' ' ) && ( line[ i ] != '\t' ) ) {
            break;
        }
    }

    // only spaces?
    if ( i == len ) {
        return 1;
    }

    // comment?
    if ( line[i] == '#' ) {
        return 1;
    }

    return 0;
}

int read_config( const char *filename )
{
    int i;

    FILE *f = fopen( filename, "r" );
    if ( f == NULL ) {
        perror( filename );
    }
    else
    {
        int line_number = 0;
        int max_line_len = 4096;
        char *line = malloc( max_line_len );

        while ( fgets( line, max_line_len, f ) != NULL )
        {
            line_number++;

            int len = strlen( line );
            if ( len == ( max_line_len - 1 ) ) {
                fprintf( stderr, "config file error: line %d too long\n", line_number );
                return -1;
            }

            // get rid of CR/LF
            for ( i = 0; i < len; i++ ) {
                if ( ( line[ i ] == '\r' ) || ( line[ i ] == '\n' ) ) {
                    line[ i ] = '\0';
                    break;
                }
            }

            if ( !line_is_empty_or_comment( line ) )
            {
                // PROCESS LINE

            }
        }
        fclose( f );
        free( line );
    }

    return 0;
}
