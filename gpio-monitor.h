#ifndef __GPIO_MONITOR_H_INCLUDED
#define __GPIO_MONITOR_H_INCLUDED

int gpio_open_value_file( int number, const char *edge_value );
int gpio_read_value_file( int fd );

void cmd_execute( const char *cmd, int gpio_number, int gpio_value );

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

extern int nodes_count;
extern monitor_node **nodes;

int read_config( const char *filename );

#endif // __GPIO_MONITOR_H_INCLUDED

