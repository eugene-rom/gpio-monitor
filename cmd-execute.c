#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <paths.h>
#include <syslog.h>

#include "gpio-monitor.h"

#define BUFSIZE 2048

void cmd_execute( monitor_node *node, int gpio_value )
{
    int pid = fork();
    if ( pid == -1 ) {
        syslog( LOG_WARNING, "fork error in cmd_execute: %m" );
    }
    else if ( pid == 0 )
    {
        char *buf = malloc( BUFSIZE );
        snprintf( buf, BUFSIZE, "%s %d %d", node->action_command, node->number, gpio_value );

        char *argp[] = { "sh", "-c", buf, NULL };
        int res = execv( _PATH_BSHELL, argp );
        if ( res == -1 ) {
            syslog( LOG_WARNING, "exec error in cmd_execute: %m" );
            _exit( 127 );
        }
    }
}
