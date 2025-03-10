#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <syslog.h>

#include "gpio-monitor.h"

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
    syslog( LOG_ERR, "config error at line %d: %s", line_number, msg );
}

static int copy_until_space( int pos, const char *source, char *dest, int maxlen, int *rlen )
{
    *rlen = 0;
    int i = 0;
    while ( ( source[ i + pos ] != ' ' )
            && ( source[ i + pos ] != '\t' )
            && ( source[ i + pos ] != 0 ) )
    {
        dest[ i ] = source[ i + pos ];
        i++;
        *rlen = i;
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

static int parse_uds_action( monitor_node *tmp, int line_number, const char *data, size_t data_maxlen )
{
    const char * const ACTION_VALUE_FORMAT_ERROR = "incorrect action value format";
    char *token;
    int i = 0;
    while ( ( token = strtok( ( i == 0 ) ? (char *)data : NULL, ":" ) ) != NULL )
    {
        if ( i == 0 )
        {
            if ( strcmp( "STREAM", token ) == 0 ) {
                tmp->socket_type = STREAM;
            }
            else if ( strcmp( "DGRAM", token ) == 0 ) {
                tmp->socket_type = DGRAM;
            }
            else {
                config_error( line_number, "incorrect socket type in action value" );
                return -1;
            }
        }
        else if ( i == 1 )
        {
            if ( strlen( token ) >= sizeof( ((struct sockaddr_un){}).sun_path ) ) {
                config_error( line_number, "too long socket pathname in action value" );
                return -1;
            }

            tmp->socket_pathname = strdup( token );
        }
        else if ( i == 2 ) {
            tmp->socket_message = strdup( token );
        }
        else {
            config_error( line_number, ACTION_VALUE_FORMAT_ERROR );
            return -1;
        }

        i++;
    }

    if ( i != 3 ) {
        config_error( line_number, ACTION_VALUE_FORMAT_ERROR );
        return -1;
    }

    return 0;
}

static int process_config_line( const char *line, int line_number )
{
    monitor_node tmp;
    memset( &tmp, 0, sizeof( tmp ) );

    int rlen, pos = 0;
    char buf[ 1024 ];

    // number
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 10, &rlen );
    if ( pos != -1 )
    {
        char *end;
        tmp.number = strtol( buf, &end, 10 );
        if ( end[0] != 0 ) {
            config_error( line_number, "incorrect number" );
            return -1;
        }
        DBG( "number = %d\n", tmp.number );
    }
    else {
        config_error( line_number, "too long number" );
        return -1;
    }

    // edge value
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 10, &rlen );
    if ( pos != -1 )
    {
        if ( rlen == 0 ) {
            config_error( line_number, "empty edge value" );
            return -1;
        }

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
    pos = copy_until_space( pos, line, buf, 2, &rlen );
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
    pos = copy_until_space( pos, line, buf, 20, &rlen );
    if ( pos != -1 )
    {
        if ( rlen == 0 ) {
            config_error( line_number, "empty debouncing delay value" );
            return -1;
        }

        char *end;
        tmp.debouncing_delay = strtol( buf, &end, 10 ) * 1000; // millis to micros
        if ( end[0] != 0 ) {
            config_error( line_number, "incorrect debouncing delay value" );
            return -1;
        }
        DBG( "debouncing_delay = %d\n", tmp.debouncing_delay );
    }
    else {
        config_error( line_number, "too long debouncing delay value" );
        return -1;
    }

    // action_type
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, 20, &rlen );
    if ( pos != -1 )
    {
        if ( rlen == 0 ) {
            config_error( line_number, "empty action type" );
            return -1;
        }

        if ( strcmp( "cmd", buf ) == 0 ) {
            tmp.action_type = CMD;
        }
        else if ( strcmp( "uds", buf ) == 0 ) {
            tmp.action_type = UDS;
        }
        else {
            config_error( line_number, "incorrect action type" );
            return -1;
        }

        DBG( "action_type = %d\n", tmp.action_type );
    }
    else {
        config_error( line_number, "incorrect action type" );
        return -1;
    }

    // command value
    memset( buf, 0, sizeof( buf ) );
    pos = skip_spaces( line, pos );
    pos = copy_until_space( pos, line, buf, sizeof( buf ) - 1, &rlen );
    if ( pos != -1 )
    {
        if ( rlen == 0 ) {
            config_error( line_number, "empty action value" );
            return -1;
        }

        if ( tmp.action_type == CMD ) {
            tmp.action_command = strdup( buf );
            DBG( "action_command = %s\n", tmp.action_command );
        }
        else if ( tmp.action_type == UDS )
        {
            if ( parse_uds_action( &tmp, line_number, buf, sizeof( buf ) ) == -1 ) {
                return -1;
            }

            DBG( "socket_type = %d\n", tmp.socket_type );
            DBG( "socket_pathname = %s\n", tmp.socket_pathname );
            DBG( "socket_message = %s\n", tmp.socket_message );
        }
        else {
            config_error( line_number, "should not happen..." );
            return -1;
        }
    }
    else {
        config_error( line_number, "too long action value" );
        return -1;
    }

    nodes_count++;
    monitor_node *node = malloc( sizeof( monitor_node ) );
    memcpy( node, &tmp, sizeof( monitor_node ) );

    nodes = realloc( nodes, sizeof( monitor_node ) * nodes_count );
    if ( nodes == NULL ) {
        syslog( LOG_ERR, "realloc failed: %m" );
        _exit( EXIT_FAILURE );
    }

    nodes[ nodes_count - 1 ] = node;

    return 0;
}

int read_config( const char *filename )
{
    int i;

    FILE *f = fopen( filename, "r" );
    if ( f == NULL ) {
        syslog( LOG_ERR, "Error open '%s': %m", filename );
        return -1;
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
                syslog( LOG_ERR, "config file error: line %d too long", line_number );
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

