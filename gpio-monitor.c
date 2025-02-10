#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <getopt.h>

#include "gpio-monitor.h"

const char * const DEFAULT_CONFIG = "/etc/gpio-monitor.conf";

int nodes_count = 0;
monitor_node **nodes = NULL;

monitor_node *find_monitor_node( int fd );

static struct option long_options[] = {
    { "config", required_argument, NULL, 'c' },
    { "help",   no_argument,       NULL, 'h' },
    { 0,        0,                 0,    0   }
};

int main( int argc, char *argv[] )
{
    const char *conffile = DEFAULT_CONFIG;

    int opt;
    while ( ( opt = getopt_long( argc, argv, "", long_options, NULL ) ) != -1 )
    {
        switch ( opt )
        {
            case 'c':
                conffile = optarg;
                break;

            case 'h':
            default: /* '?' */
                fprintf( stderr,
                    "Usage: %s [OPTION]\n\n" \
                    "--config=CONFIG_FILE   path to config file\n" \
                    "--help                 display this short help and exit\n\n" \
                    "With no CONFIG_FILE, '%s' used.\n",
                    argv[0], DEFAULT_CONFIG );
                exit( ( opt == '?' ) ? EXIT_FAILURE : EXIT_SUCCESS );
        }
    }

    printf( "Using configuration file '%s'\n", conffile );
    if ( read_config( conffile ) == -1 ) {
        return 1;
    }

    printf( "Monitoring numbers: " );
    int i;
    for ( i = 0; i < nodes_count; i++ ) {
        printf( ( i == 0 ) ? "%d" : ", %d", nodes[i]->number );
    }
    printf( ".\n" );

    printf( "Open file descriptors: " );
    fflush( stdout );
    for ( i = 0; i < nodes_count; i++ )
    {
        nodes[i]->fd = gpio_open_value_file( nodes[i]->number, nodes[i]->edge_value );
        if ( nodes[ i ]->fd == -1 ) {
            return 0;
        }
        printf( ( i == 0 ) ? "%d" : ", %d", nodes[i]->fd );
    }
    printf( ".\n" );

    struct pollfd *pfds = calloc( nodes_count, sizeof( struct pollfd ) );
    for ( i = 0; i < nodes_count; i++ ) {
        pfds[i].fd = nodes[i]->fd;
        pfds[i].events = POLLPRI | POLLERR;
    }

    printf( "Polling for events...\n" );
    while ( 1 )
    {
        int ready = poll( pfds, nodes_count, -1 );
        if ( ready == -1 ) {
            perror( "poll error" );
            break;
        }

        for ( i = 0; i < nodes_count; i++ )
        {
            if ( pfds[i].revents != 0 )
            {
                if ( pfds[i].revents & POLLPRI )
                {
                    monitor_node *node = find_monitor_node( pfds[i].fd );
                    if ( node != NULL )
                    {
                        int val, val2;
                        val = val2 = gpio_read_value_file( pfds[i].fd );
                        if ( node->debouncing_delay != 0 ) {
                            usleep( node->debouncing_delay );
                            val2 = gpio_read_value_file( pfds[i].fd );
                        }

                        if ( ( ( val != -1 ) && ( val == val2 ) )
                             && ( ( val == node->expected_value ) || ( node->expected_value == -1 ) ) ) {
                            cmd_execute( node->action_command, node->number, val );
                        }
                    }
                }
                else if ( pfds[i].revents & POLLERR ) {
                    fprintf( stderr, "POLLERR fd: %d\n", pfds[i].fd );
                }
            }
        }
    }
    return 0;
}

monitor_node *find_monitor_node( int fd )
{
    int i;
    for ( i = 0; i < nodes_count; i++ ) {
        if ( nodes[i]->fd == fd ) {
            return nodes[i];
        }
    }
    return NULL;
}
