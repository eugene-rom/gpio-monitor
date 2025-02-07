#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <paths.h>

#define BUFSIZE 2048

void cmd_execute( const char *cmd, int gpio_number, int gpio_value )
{
    int pid = fork();
    if ( pid == -1 ) {
        perror( "fork error in cmd_execute" );
    }
    else if ( pid == 0 )
    {
        char *buf = malloc( BUFSIZE );
        snprintf( buf, BUFSIZE, "%s %d %d", cmd, gpio_number, gpio_value );

        char *argp[] = { "sh", "-c", buf, NULL };
        int res = execv( _PATH_BSHELL, argp );
        if ( res == -1 ) {
            perror( "exec error in cmd_execute" );
            _exit( 127 );
        }
    }
}
