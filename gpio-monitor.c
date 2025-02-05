#include <stdio.h>
#include <stdlib.h>
#include <poll.h>

#include "gpio-monitor.h"

//  IN DEVELOPMENT
//     TEST CODE
// NON-FUNCTIONAL YET

int main( int argc, char *argv[] )
{
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
