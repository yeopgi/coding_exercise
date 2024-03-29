#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

int transfer_byte(int from, int to, int is_control);

typedef struct {char *name; int flag; } speed_spec;


void print_status(int fd) {
	int status;
	unsigned int arg;
	status = ioctl(fd, TIOCMGET, &arg);
	fprintf(stderr, "[STATUS]: ");
	if(arg & TIOCM_RTS) fprintf(stderr, "RTS ");
	if(arg & TIOCM_CTS) fprintf(stderr, "CTS ");
	if(arg & TIOCM_DSR) fprintf(stderr, "DSR ");
	if(arg & TIOCM_CAR) fprintf(stderr, "DCD ");
	if(arg & TIOCM_DTR) fprintf(stderr, "DTR ");
	if(arg & TIOCM_RNG) fprintf(stderr, "RI ");
	fprintf(stderr, "\r\n");
}

int main(int argc, char *argv[])
{
	int comfd;
	struct termios oldtio, newtio;       //place for old and new port settings for serial port
	struct termios oldkey, newkey;       //place tor old and new port settings for keyboard teletype
	char *devicename = argv[1];
	int need_exit = 0;
	speed_spec speeds[] =
	{
		{"1200", B1200},
		{"2400", B2400},
		{"4800", B4800},
		{"9600", B9600},
		{"19200", B19200},
		{"38400", B38400},
		{"57600", B57600},
		{"115200", B115200},
		{NULL, 0}
	};
	int speed = B9600;

	if(argc < 2) {
		fprintf(stderr, "example: %s /dev/ttyS0 [115200]\n", argv[0]);
		exit(1);
	}

	comfd = open(devicename, O_RDWR | O_NONBLOCK);
	if (comfd < 0)
	{
		perror(devicename);
		exit(-1);
	}

	if(argc > 2) {	
		speed_spec *s;
		for(s = speeds; s->name; s++) {
			if(strcmp(s->name, argv[2]) == 0) {
				speed = s->flag;
				fprintf(stderr, "setting speed %s\n", s->name);
				break;
			}
		}
	}

	fprintf(stderr, "C-a exit, C-x modem lines status\n");

	tcgetattr(STDIN_FILENO,&oldkey);
	newkey.c_cflag = B9600 | CRTSCTS | CS8 | CLOCAL | CREAD;
	newkey.c_iflag = IGNPAR;
	newkey.c_oflag = 0;
	newkey.c_lflag = 0;
	newkey.c_cc[VMIN]=2;
	newkey.c_cc[VTIME]=0;
	tcflush(STDIN_FILENO, TCIFLUSH);
	tcsetattr(STDIN_FILENO,TCSANOW,&newkey);

	tcgetattr(comfd,&oldtio); // save current port settings 
	newtio.c_cflag = speed | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = ECHO;
	newtio.c_cc[VMIN]=1;
	newtio.c_cc[VTIME]=0;
	tcflush(comfd, TCIFLUSH);
	tcsetattr(comfd,TCSANOW,&newtio);

	//print_status(comfd);

	while(!need_exit) {
		fd_set fds;
		int ret;
		
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		FD_SET(comfd, &fds);

		puts("prev select");

		ret = select(comfd+1, &fds, NULL, NULL, NULL);
		puts("post select");
		if(ret == -1) {
			perror("select");
		} else if (ret > 0) {
			if(FD_ISSET(STDIN_FILENO, &fds)) {
				need_exit = transfer_byte(STDIN_FILENO, comfd, 1);
				puts("in stdin");
			}
			if(FD_ISSET(comfd, &fds)) {
				need_exit = transfer_byte(comfd, STDIN_FILENO, 0);
				puts("in comfd");
			}
		}
	}

	tcsetattr(comfd,TCSANOW,&oldtio);
	tcsetattr(STDIN_FILENO,TCSANOW,&oldkey);
	close(comfd);

	return 0;
}


int transfer_byte(int from, int to, int is_control) {
	char c;
	int ret;
	do {
		ret = read(from, &c, 1);
		printf("from : %d\n", from);
		printf("to : %d\n", to);
	} while (ret < 0 && errno == EINTR);
	if(ret == 1) {
		if(is_control) {
			if(c == '\x01') { // C-a
				return -1;
			} else if(c == '\x18') { // C-x
				print_status(to);
				return 0;
			}
		}
		while(write(to, &c, 1) == -1) {
			if(errno!=EAGAIN && errno!=EINTR) {
				 perror("write failed"); 
				 
				 break; 
			}
		}
	} else {
		fprintf(stderr, "\nnothing to read. probably port disconnected.\n");
		return -2;
	}
	return 0;
}