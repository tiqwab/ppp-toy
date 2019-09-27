#include <errno.h>
#include <fcntl.h>
#include <linux/ppp-ioctl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "tty.h"

#define DEV_PPP_PATH "/dev/ppp"

static int tty_fd = -1;
static int ppp_fd = -1;
static int dev_ppp_fd = -1;

static void print_error(char *func_name, char *msg) {
    fprintf(stderr, "%s. ", msg);
    perror(func_name);
}

static void intr_sigint(int signum) {
    if (tty_fd >= 0) {
        int ppp_discipline = N_TTY;
        if (ioctl(tty_fd, TIOCSETD, &ppp_discipline) < 0) {
            fprintf(stderr, "Failed in ioctl with TIOCSETD (N_TTY). ");
            perror("ioctl");
        }
    }
}


/*
 * Return a file descriptor to write and read ppp frame or -1 if error.
 */
int setup_tty_and_ppp_if(char *tty_path) {
    int ppp_discipline;
    int if_unit = -1;
    int ch_index = -1;
    struct sigaction sa;

    // (1) Open tty.
    if ((tty_fd = open(tty_path, O_RDWR)) < 0) {
        print_error("open", "Failed to open tty");
        return -1;
    }

    // (2) Set tty discipline.
    ppp_discipline = N_SYNC_PPP;
    if (ioctl(tty_fd, TIOCSETD, &ppp_discipline) < 0) {
        print_error("ioctl", "Failed in ioctl with TIOCSETD (N_SYNC_PPP)");
        return -1;
    }

    // To reset line discipline
    sa.sa_handler = intr_sigint;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        print_error("sigaction", "Failed to set sigaction to SIGINT");
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        print_error("sigaction", "Failed to set sigaction to SIGTERM");
        return -1;
    }

    // Open an instance of /dev/ppp and connect the channel to it.
    if (ioctl(tty_fd, PPPIOCGCHAN, &ch_index) < 0) {
        print_error("ioctl", "Failed in ioctl with TIOPPPIOCGCHAN");
        return -1;
    }
    printf("ch_index: %d\n", ch_index);

    // (3) Open /dev/ppp.
    if ((ppp_fd = open(DEV_PPP_PATH, O_RDWR)) < 0) {
        print_error("open", "Failed to open /dev/ppp for ppp_fd");
        return -1;
    }

    if (ioctl(ppp_fd, PPPIOCATTCHAN, &ch_index) < 0) {
        print_error("ioctl", "Failed in ioctl with PPPIOCATTCHAN");
        return -1;
    }

    // (4) Create a new PPP unit.
    // A ppp network interface is created.
    if ((dev_ppp_fd = open(DEV_PPP_PATH, O_RDWR)) < 0) {
        print_error("open", "Failed to open /dev/ppp for dev_ppp_fd");
        return -1;
    }

    if (ioctl(dev_ppp_fd, PPPIOCNEWUNIT, &if_unit) < 0) {
        print_error("ioctl", "Failed in ioctl with PPPIOCNEWUNIT");
        return -1;
    }
    printf("if_unit: %d\n", if_unit);

    // (5) Attach an channel to PPP unit.
    if (ioctl(ppp_fd, PPPIOCCONNECT, &if_unit) < 0) {
        print_error("ioctl", "Failed in ioctl with PPPIOCCONNECT");
        return -1;
    }

    return dev_ppp_fd;
}
