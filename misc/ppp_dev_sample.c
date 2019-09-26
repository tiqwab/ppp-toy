#include <errno.h>
#include <fcntl.h>
#include <linux/ppp-ioctl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#define DEV_PPP_PATH "/dev/ppp"
#define SC_RCVB (SC_RCV_B7_0 | SC_RCV_B7_1 | SC_RCV_EVNP | SC_RCV_ODDP)
#define SC_LOGB (SC_DEBUG | SC_LOG_INPKT | SC_LOG_OUTPKT | SC_LOG_RAWIN | SC_LOG_FLUSH)

static int tty_fd = -1;

static void perror_exit(char *func_name, char *msg) {
    fprintf(stderr, "%s. ", msg);
    perror(func_name);
    exit(EXIT_FAILURE);
}

/*
 * modify_flags - set and clear flag bits controlling the kernel PPP driver.
 *
 * from modify_flags at https://github.com/paulusmack/ppp/blob/master/pppd/sys-linux.c#L289
 */
static int modify_flags(int fd, int clear_bits, int set_bits)
{
    int flags;

    if (ioctl(fd, PPPIOCGFLAGS, &flags) == -1)
        goto err;
    flags = (flags & ~clear_bits) | set_bits;
    if (ioctl(fd, PPPIOCSFLAGS, &flags) == -1)
        goto err;

    return 0;

 err:
    if (errno != EIO) {
        fprintf(stderr, "Failed to set PPP kernel option flags. ");
        perror("iocl");
    }
    return -1;
}

static void intr_sigint(int signum) {
    printf("intr_sigint\n");
    if (tty_fd >= 0) {
        int ppp_discipline = N_TTY;
        if (ioctl(tty_fd, TIOCSETD, &ppp_discipline) < 0) {
            fprintf(stderr, "Failed in ioctl with TIOCSETD (N_TTY). ");
            perror("ioctl");
        }
    }
}


/*
 * Example of usage of /dev/ppp.
 *
 * gcc -o misc/ppp_dev_sample -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE misc/ppp_dev_sample.c
 */
int main(int argc, char *argv[]) {
    int ppp_fd;
    int dev_ppp_fd;
    char *tty_path;
    int ppp_discipline;
    int if_unit = -1;
    int init_flags;
    int ch_index = -1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s TTY\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    tty_path = argv[1];

    // (1) Open tty.
    if ((tty_fd = open(tty_path, O_RDWR)) < 0) {
        perror_exit("open", "Failed to open tty");
    }

    // Change tty to an exclusive mode.
    // ioctl(tty_fd, TIOCNXCL);
    // if (ioctl(tty_fd, TIOCEXCL) < 0) {
    //     perror_exit("ioctl", "Failed in ioctl with TIOCEXCL");
    // }

    // (2) Set tty discipline.
    ppp_discipline = N_SYNC_PPP;
    if (ioctl(tty_fd, TIOCSETD, &ppp_discipline) < 0) {
        perror_exit("ioctl", "Failed in ioctl with TIOCSETD (N_SYNC_PPP)");
    }

    // To reset line discipline
    struct sigaction sa;
    sa.sa_handler = intr_sigint;
    sigemptyset(&sa.sa_mask);
    // sa.sa_flags = SA_RESTART; /* Restart functions if interrupted by handler */
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror_exit("sigaction", "Failed to set sigaction to SIGINT");
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror_exit("sigaction", "Failed to set sigaction to SIGTERM");
    }

    // Open an instance of /dev/ppp and connect the channel to it.
    if (ioctl(tty_fd, PPPIOCGCHAN, &ch_index) < 0) {
        perror_exit("ioctl", "Failed in ioctl with TIOPPPIOCGCHAN");
    }
    printf("ch_index: %d\n", ch_index);

    // (3) Open /dev/ppp.
    if ((ppp_fd = open(DEV_PPP_PATH, O_RDWR)) < 0) {
        perror_exit("open", "Failed to open /dev/ppp for ppp_fd");
    }

    // if (fcntl(ppp_fd, F_SETFD, FD_CLOEXEC) < 0) {
    //     perror_exit("fcntl", "Failed in fcntl with F_SETFD");
    // }

    if (ioctl(ppp_fd, PPPIOCATTCHAN, &ch_index) < 0) {
        perror_exit("ioctl", "Failed in ioctl with PPPIOCATTCHAN");
    }

    // init_flags = fcntl(ppp_fd, F_GETFL);
    // if (init_flags == -1 || fcntl(ppp_fd, F_SETFL, init_flags | O_NONBLOCK) == -1) {
    //     perror_exit("ioctl", "Failed in ioctl to set O_NONBLOCK");
    // }

    // (4) Create a new PPP unit.
    // A ppp network interface is created.
    if ((dev_ppp_fd = open(DEV_PPP_PATH, O_RDWR)) < 0) {
        perror_exit("open", "Failed to open /dev/ppp for dev_ppp_fd");
    }

    // init_flags = fcntl(dev_ppp_fd, F_GETFL);
    // if (init_flags == -1 || fcntl(dev_ppp_fd, F_SETFL, init_flags | O_NONBLOCK) == -1) {
    //     perror_exit("ioctl", "Failed in ioctl to set O_NONBLOCK");
    // }

    if (ioctl(dev_ppp_fd, PPPIOCNEWUNIT, &if_unit) < 0) {
        perror_exit("ioctl", "Failed in ioctl with PPPIOCNEWUNIT");
    }
    printf("if_unit: %d\n", if_unit);

    // (5) Attach an channel to PPP unit.
    if (ioctl(ppp_fd, PPPIOCCONNECT, &if_unit) < 0) {
        perror_exit("ioctl", "Failed in ioctl with PPPIOCCONNECT");
    }

    // modify_flags(ppp_fd, SC_RCVB | SC_LOGB, SC_LOGB);
    // modify_flags(ppp_fd, SC_COMP_PROT | SC_COMP_AC | SC_SYNC, SC_SYNC);

    // u_int32_t mru = 1480;
    // ioctl(ppp_fd, PPPIOCSMRU, &mru);

    // u_int32_t asyncmap = 0xffffffff;
    // ioctl(ppp_fd, PPPIOCSASYNCMAP, &asyncmap);

    /*
     * Do something.
     */

    unsigned char buf[4096];
    int n;
    while (1) {
        if ((n = read(dev_ppp_fd, buf, 4096)) < 0) { // ppp_fd cannot be used here.
            if (errno != EAGAIN && errno != EINTR) {
                perror("read");
                break;
            }
        } else if (n == 0) {
            break;
        } else {
            for (int i = 0; i < n; i++) {
                printf("%02x ", buf[i]);
            }
            printf("\n");

            if (write(dev_ppp_fd, buf, n) < 0) { // ppp_fd can be also used here
                perror("write");
                break;
            }
        }
    }

    /*
     * Clean up
     */

    if (close(dev_ppp_fd) < 0) {
        perror_exit("close", "Failed to close /dev/ppp");
    }

    if (close(ppp_fd) < 0) {
        perror_exit("close", "Failed to close /dev/ppp");
    }

    if (close(tty_fd) < 0) {
        perror_exit("close", "Failed to close tty");
    }

    return 0;
}
