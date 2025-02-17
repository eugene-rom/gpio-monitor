#ifndef __GPIO_MONITOR_H_INCLUDED
#define __GPIO_MONITOR_H_INCLUDED

int gpio_open_value_file( int number, const char *edge_value );
int gpio_read_value_file( int fd );

#define DBG( format, arg )
//#define DBG( format, arg ) printf( format, arg )

enum actionType { CMD, UDS };
enum socketType { STREAM, DGRAM };

typedef struct _monitor_node
{
    int number;
    int fd;
    const char *edge_value;
    int expected_value;
    __useconds_t debouncing_delay;
    enum actionType action_type;

    // if action_type == CMD
    const char *action_command;

    // if action_type == UDS
    enum socketType socket_type;
    const char *socket_pathname;
    const char *socket_message;
} monitor_node;

extern int nodes_count;
extern monitor_node **nodes;

int read_config( const char *filename );

void cmd_execute( monitor_node *node, int gpio_value );
void uds_message( monitor_node *node, int gpio_value );

#endif // __GPIO_MONITOR_H_INCLUDED

