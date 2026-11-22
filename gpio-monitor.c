#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h>

#include "gpio-monitor.h"

const char * const DEFAULT_CONFIG = "/etc/gpio-monitor.conf";

const int DEBOUNCING_DELAY_FRACTION = 2000; // in microseconds

int nodes_count = 0;
monitor_node **nodes = NULL;

static int64_t get_current_ms();
static monitor_node *find_monitor_node( int fd );

static struct option long_options[] = {
    { "config",      required_argument, NULL, 'c' },
    { "quiet",       no_argument,       NULL, 'q' },
    { "test-action", required_argument, NULL, 't' },
    { "help",        no_argument,       NULL, 'h' },
    { 0,             0,                 0,    0   }
};

static void execute_action( monitor_node *node, int gpio_value )
{
    if ( node->action_type == CMD ) {
        cmd_execute( node, gpio_value );
    }
    else if ( node->action_type == UDS ) {
        uds_message( node, gpio_value );
    }
}

int main( int argc, char *argv[] )
{
    const char *conffile = DEFAULT_CONFIG;
    int test_number = -1;
    int quiet = 0;
    int i;

    int opt;
    while ( ( opt = getopt_long( argc, argv, "", long_options, NULL ) ) != -1 )
    {
        switch ( opt )
        {
            case 'c':
                conffile = optarg;
                break;

            case 'q':
                quiet = 1;
                break;

            case 't':
                test_number = strtol( optarg, NULL, 10 );
                break;

            case 'h':
            default: /* '?' */
                fprintf( stderr,
                    "Usage: %s [OPTION]\n\n" \
                    "--config=CONFIG_FILE   path to config file\n" \
                    "--quiet                no messages to console (only to syslog)\n" \
                    "--test-action=NUMBER   execute action for specified GPIO number and quit\n" \
                    "--help                 display this short help and exit\n\n" \
                    "With no CONFIG_FILE, '%s' used.\n",
                    argv[0], DEFAULT_CONFIG );
                exit( ( opt == '?' ) ? EXIT_FAILURE : EXIT_SUCCESS );
        }
    }

    int syslog_option = quiet ? 0 : LOG_CONS | LOG_PERROR;
    openlog( NULL, syslog_option, LOG_USER );

    syslog( LOG_INFO, "Using configuration file '%s'", conffile );
    if ( read_config( conffile ) == -1 ) {
        return 1;
    }

    // prevent unattended child processes from becoming zombies
    signal( SIGCHLD, SIG_IGN );

    if ( test_number != -1 )
    {
        printf( "executing action for number %d...\n", test_number );

        int executed = 0;
        for ( i = 0; i < nodes_count; i++ )
        {
            if ( nodes[i]->number == test_number ) {
                execute_action( nodes[i], 0 );
                executed = 1;
                break;
            }
        }

        if ( executed ) {
            wait( NULL ); // action forked, so wait
            printf( "action executed.\n" );
        }
        else {
            printf( "number %d not defined in config.\n", test_number );
        }

        return 0;
    }

    for ( i = 0; i < nodes_count; i++ )
    {
        // for debounced pins, monitor both edges in sysfs to track press and release
        // state transitions in last_value; nodes[i]->edge_value keeps the configured edge
        const char *sysfs_edge = nodes[i]->edge_value;
        if ( ( nodes[i]->debouncing_delay > 0 ) && ( strcmp( sysfs_edge, "none" ) != 0 ) ) {
            sysfs_edge = "both";
        }

        nodes[i]->fd = gpio_open_value_file( nodes[i]->number, sysfs_edge );
        if ( nodes[ i ]->fd == -1 ) {
            return 0;
        }

        nodes[i]->last_value = gpio_read_value_file( nodes[i]->fd );

        syslog( LOG_INFO,
                "Monitoring number: %d, file descriptor: %d",
                nodes[i]->number, nodes[i]->fd );
    }

    struct pollfd *pfds = calloc( nodes_count, sizeof( struct pollfd ) );
    for ( i = 0; i < nodes_count; i++ ) {
        pfds[i].fd = nodes[i]->fd;
        pfds[i].events = POLLPRI | POLLERR;
    }

    syslog( LOG_INFO, "Polling for events..." );
    while ( 1 )
    {
        int ready = poll( pfds, nodes_count, -1 );
        if ( ready == -1 )
        {
            if ( errno == EINTR ) {
                continue;
            }
            syslog( LOG_ERR, "poll error: %m" );
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
                        int val = gpio_read_value_file( pfds[i].fd );
                        if ( val == -1 )
                        {
                            syslog( LOG_ERR, "Disabling monitoring for gpio %d (fd: %d) due to read error",
                                    node->number, pfds[i].fd );
                            close( pfds[i].fd );
                            pfds[i].fd = -1;
                            continue;
                        }

                        if ( node->debouncing_delay != 0 )
                        {
                            int64_t t = get_current_ms();

                            do
                            {
                                usleep( DEBOUNCING_DELAY_FRACTION );
                                int val2 = gpio_read_value_file( pfds[i].fd );
                                if ( val2 == -1 ) {
                                    val = -1;
                                    break;
                                }
                                if ( val2 != val ) {
                                    val = val2;
                                    t = get_current_ms();
                                }
                            }
                            while ( ( get_current_ms() - t ) < node->debouncing_delay );

                            if ( val == -1 )
                            {
                                syslog( LOG_ERR, "Disabling monitoring for gpio %d (fd: %d) due to read error",
                                        node->number, pfds[i].fd );
                                close( pfds[i].fd );
                                pfds[i].fd = -1;
                                continue;
                            }
                        }

                        if ( ( node->debouncing_delay > 0 ) || ( strcmp( node->edge_value, "both" ) == 0 ) )
                        {
                            if ( val == node->last_value ) {
                                continue;
                            }
                            node->last_value = val;
                        }

                        if ( ( strcmp( node->edge_value, "falling" ) == 0 ) && ( val != 0 ) ) {
                            continue;
                        }
                        if ( ( strcmp( node->edge_value, "rising" ) == 0 ) && ( val != 1 ) ) {
                            continue;
                        }

                        if ( ( node->expected_value == -1 ) || ( val == node->expected_value ) ) {
                            execute_action( node, val );
                        }
                    }
                }
                else if ( pfds[i].revents & ( POLLERR | POLLHUP | POLLNVAL ) ) {
                    syslog( LOG_ERR, "POLLERR/HUP/NVAL on fd %d, disabling monitoring", pfds[i].fd );
                    close( pfds[i].fd );
                    pfds[i].fd = -1;
                }
            }
        }
    }

    return 0;
}

static monitor_node *find_monitor_node( int fd )
{
    int i;
    for ( i = 0; i < nodes_count; i++ ) {
        if ( nodes[i]->fd == fd ) {
            return nodes[i];
        }
    }
    return NULL;
}

static int64_t get_current_ms()
{
    struct timespec spec;
    clock_gettime( CLOCK_MONOTONIC, &spec );

    return (int64_t)spec.tv_sec * 1000 + (int64_t)spec.tv_nsec / 1000000;
}

