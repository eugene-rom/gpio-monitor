#ifndef __GPIO_MONITOR_H_INCLUDED
#define __GPIO_MONITOR_H_INCLUDED

int gpio_open_value_file( int number );
int gpio_read_value_file( int fd );

void cmd_execute( const char *cmd, int gpio_number, int gpio_value );

#endif // __GPIO_MONITOR_H_INCLUDED

