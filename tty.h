#ifndef __TTY_H__
#define __TTY_H__

#define PPP_DEVICE_NAME_SIZE 32

extern char ppp_device_name[PPP_DEVICE_NAME_SIZE];

int setup_tty_and_ppp_if(char *tty_path);

#endif /* __TTY_H__ */
