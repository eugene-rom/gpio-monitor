#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>

#include "gpio-monitor.h"

//  IN DEVELOPMENT
//     TEST CODE
// NON-FUNCTIONAL YET

enum actionType { CMD };

typedef struct _monitor_node
{
    int number;
    int fd;
    const char *edge_value;
    int expected_value;
    enum actionType action_type;
    const char *action_command;
} monitor_node;

static int nodes_count = 0;
static monitor_node nodes[];

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

static int line_is_empty_or_comment( const char *line )
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

static int copy_until_space( int pos, const char *source, char *dest, int maxlen )
{
    int i = 0;
    while ( ( source[ i + pos ] != ' ' ) && ( source[ i + pos ] != '\t' ) )
    {
        dest[ i ] = source[ i + pos ];
        i++;
        if ( i >= maxlen ) {
            return -1;
        }
    }
    return i + pos;
}

static int skip_spaces( const char *line, int pos )
{
    while ( ( line[ pos ] == ' ' ) || ( line[ pos ] == '\t' ) ) {
        pos++;
    }

    return pos;
}

static int process_config_line( const char *line, int line_number )
{
    monitor_node tmp;
    int pos = 0;
    char buf[ 1024 ];

    // number
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 10 );
    if ( pos != -1 ) {
        char *end;
        tmp.number = strtol( buf, &end, 10 );
        printf( "number = %d\n", tmp.number );
    }
    else {
        fprintf( stderr, "config file error in line %d: too long number\n", line_number );
        return -1;
    }

    // edge value
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 10 );
    if ( pos != -1 ) {
        tmp.edge_value = strdup( buf );
        printf( "edge_value = %s\n", tmp.edge_value );
    }
    else {
        fprintf( stderr, "config file error in line %d: too long edge value\n", line_number );
        return -1;
    }

    // expected value
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 2 );
    if ( pos != -1 )
    {
        if ( buf[0] == '0' ) {
            tmp.expected_value = 0;
        }
        else if ( buf[0] == '1' ) {
            tmp.expected_value = 1;
        }
        else if ( buf[0] == '*' ) {
            tmp.expected_value = -1;
        }
        else {
            fprintf( stderr, "config file error in line %d: incorrect expected value\n", line_number );
            return -1;
        }

        printf( "expected_value = %d\n", tmp.expected_value );
    }
    else {
        fprintf( stderr, "config file error in line %d: too long expected value\n", line_number );
        return -1;
    }

    // action_type
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 20 );
    if ( pos != -1 )
    {
        if ( strcmp( "cmd", buf ) == 0 ) {
            tmp.action_type = CMD;
            printf( "action_type = %d\n", tmp.action_type );
        }
        else {
            fprintf( stderr, "config file error in line %d: incorrect action type\n", line_number );
        }
    }
    else {
        fprintf( stderr, "config file error in line %d: too long action type\n", line_number );
        return -1;
    }

    //monitor_node *node = malloc( sizeof( monitor_node ) );
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

            if ( !line_is_empty_or_comment( line ) ) {
                if ( process_config_line( line, line_number ) == -1 ) {
                    return -1;
                }
            }
        }
        fclose( f );
        free( line );
    }

    return 0;
}
