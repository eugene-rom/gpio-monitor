#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static const char* SYSFS_GPIO_EXPORT             = "/sys/class/gpio/export";
//static const char* SYSFS_GPIO_UNEXPORT           = "/sys/class/gpio/unexport";
static const char* SYSFS_GPIO_DIRECTION_TEMPLATE = "/sys/class/gpio/gpio%d/direction";
static const char* SYSFS_GPIO_EDGE_TEMPLATE      = "/sys/class/gpio/gpio%d/edge";
static const char* SYSFS_GPIO_VALUE_TEMPLATE     = "/sys/class/gpio/gpio%d/value";
static const char* DIRECTION_IN                  = "in\n";

static void ferrmsg( const char *operation, const char *filename ) {
    fprintf( stderr, "%s error '%s': %s.\n", operation, filename, strerror( errno ) );
}

static int write_control_file( const char *name, const char *data )
{
    int fd = open( name, O_WRONLY | O_TRUNC );
    if ( fd == -1 ) {
        ferrmsg( "Open", name );
        return -1;
    }

    if ( write( fd, data, strlen( data ) ) == -1 ) {
        ferrmsg( "Write", name );
        close( fd );
        return -1;
    }

    if ( close( fd ) == -1 ) {
        ferrmsg( "Close", name );
        return -1;
    }

    return 0;
}

static int check_control_value( int number, const char *nametemplate, const char *value )
{
    char *filename = malloc( PATH_MAX );
    snprintf( filename, PATH_MAX, nametemplate, number );

    int fd = open( filename, O_RDWR );
    if ( fd == -1 ) {
        ferrmsg( "Open", filename );
        free( filename );
        return -1;
    }

    char buf[ 8 ];
    memset( buf, 0, sizeof( buf ) );
    if ( read( fd, buf, sizeof( buf ) - 1 ) == -1 ) {
        ferrmsg( "Read", filename );
        close( fd );
        free( filename );
        return -1;
    }

    if ( strcmp( value, buf ) != 0 )
    {
        if ( write( fd, value, strlen( value ) ) == -1 ) {
            ferrmsg( "Write", filename );
            close( fd );
            free( filename );
            return -1;
        }
    }

    close( fd );
    free( filename );
    return 0;
}

static int check_direction_value( int number ) {
    return check_control_value( number,
                SYSFS_GPIO_DIRECTION_TEMPLATE, DIRECTION_IN );
}

static int check_edge_value( int number, const char *edge_value ) {
    return check_control_value( number, SYSFS_GPIO_EDGE_TEMPLATE, edge_value );
}

static int do_export( int number ) {
    char nchar[ 10 ];
    snprintf( nchar, sizeof( nchar ), "%d", number );
    return write_control_file( SYSFS_GPIO_EXPORT, nchar );
}

int gpio_open_value_file( int number, const char *edge_value )
{
    char *filename = malloc( PATH_MAX );
    snprintf( filename, PATH_MAX, SYSFS_GPIO_VALUE_TEMPLATE, number );

    // check if number exported
    if ( access( filename, F_OK ) != 0 )
    {
        // not exported, do export
        if ( do_export( number ) != 0 ) {
            fprintf( stderr, "Unable to export gpio number '%d'\n", number );
            free( filename );
            return -1;
        }
    }

    if ( check_direction_value( number ) != 0 ) {
        fprintf( stderr, "Unable to switch gpio number %d direction\n", number );
        free( filename );
        return -1;
    }

    if ( check_edge_value( number, edge_value ) != 0 ) {
        fprintf( stderr, "Unable to switch gpio number %d edge\n", number );
        free( filename );
        return -1;
    }

    int fd = open( filename, O_RDONLY );
    if ( fd == -1 ) {
        ferrmsg( "Open", filename );
        free( filename );
        return -1;
    }

    free( filename );
    return fd;
}

int gpio_read_value_file( int fd )
{
    if ( lseek( fd, 0, SEEK_SET ) == -1 ) {
        fprintf( stderr, "Seek error, fd %d: %s.\n", fd, strerror( errno ) );
        return -1;
    }

    char buf[ 8 ];
    memset( buf, 0, sizeof( buf ) );
    if ( read( fd, buf, sizeof( buf ) - 1 ) == -1 ) {
        fprintf( stderr, "Read value error, fd %d: %s.\n", fd, strerror( errno ) );
        return -1;
    }

    // value expected '0' or '1'
    if ( ( buf[0] != '0' ) && ( buf[0] != '1' ) ) {
        fprintf( stderr, "Unexpected value from fd %d: %s.\n", fd, buf );
        return -1;
    }

    return buf[0] - '0';
}


