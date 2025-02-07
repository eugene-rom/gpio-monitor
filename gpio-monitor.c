#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "gpio-monitor.h"

#define DBG( format, arg )
//#define DBG( format, arg ) printf( format, arg )

enum actionType { CMD };

typedef struct _monitor_node
{
    int number;
    int fd;
    const char *edge_value;
    int expected_value;
    __useconds_t debouncing_delay;
    enum actionType action_type;
    const char *action_command;
} monitor_node;

static int nodes_count = 0;
static monitor_node **nodes = NULL;

int read_config( const char *filename );
monitor_node *find_monitor_node( int fd );

int main( int argc, char *argv[] )
{
    // test reading config file
    if ( read_config( "gpio-monitor.conf" ) == -1 ) {
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

//
// configuration parse section below
//
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

static void config_error( int line_number, const char *msg ) {
    fprintf( stderr, "config error at line %d: %s\n", line_number, msg );
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
    memset( &tmp, 0, sizeof( tmp ) );

    int pos = 0;
    char buf[ 1024 ];

    // number
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 10 );
    if ( pos != -1 ) {
        char *end;
        tmp.number = strtol( buf, &end, 10 );
        DBG( "number = %d\n", tmp.number );
    }
    else {
        config_error( line_number, "too long number" );
        return -1;
    }

    // edge value
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 10 );
    if ( pos != -1 ) {
        tmp.edge_value = strdup( buf );
        DBG( "edge_value = %s\n", tmp.edge_value );
    }
    else {
        config_error( line_number, "incorrect edge value" );
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
            config_error( line_number, "incorrect expected value" );
            return -1;
        }

        DBG( "expected_value = %d\n", tmp.expected_value );
    }
    else {
        config_error( line_number, "incorrect expected value" );
        return -1;
    }

    // debounce value
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 20 );
    if ( pos != -1 ) {
        char *end;
        tmp.debouncing_delay = strtol( buf, &end, 10 ) * 1000;
        DBG( "debouncing_delay = %d\n", tmp.debouncing_delay );
    }
    else {
        config_error( line_number, "too long debouncing delay value" );
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
            DBG( "action_type = %d\n", tmp.action_type );
        }
        else {
            config_error( line_number, "incorrect action type" );
        }
    }
    else {
        config_error( line_number, "incorrect action type" );
        return -1;
    }

    // command value
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, sizeof( buf ) - 1 );
    if ( pos != -1 ) {
        tmp.action_command = strdup( buf );
        DBG( "action_command = %s\n", tmp.action_command );
    }
    else {
        config_error( line_number, "too long command value" );
        return -1;
    }

    nodes_count++;
    monitor_node *node = malloc( sizeof( monitor_node ) );
    memcpy( node, &tmp, sizeof( monitor_node ) );

    nodes = realloc( nodes, sizeof( monitor_node ) * nodes_count );
    if ( nodes == NULL ) {
        perror( "realloc failed" );
        _exit(1);
    }

    nodes[ nodes_count - 1 ] = node;

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
