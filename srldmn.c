/*
 *
 * Basic Serial Port Daemon
 *
 */

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define PORT_0 "/dev/ttyUSB0"
#define PORT_1 "/dev/ttyUSB1"

int fd0, fd1;

void serialLoop (int fd0, int fd1)
{
	char buf[256];

	int n;
	int i;

	for (;;)
	{
		n = read(fd0, &buf[0], sizeof(buf)-1);
		if (n == -1)
		{
			printf ("  unable to read from:  %s\n", PORT_0);
			return;
		}

		if (n)
		{
			// process received characters
			for (i=0; i<n; ++i)
			{
  				if (write(fd1, &buf[i], 1) == -1)
				{
					printf ("  unable to write to:  %s\n", PORT_1);
					return;
				}
			}
		}

		n = read(fd1, &buf[0], sizeof(buf)-1);
		if (n == -1)
		{
			printf ("  unable to read from:  %s\n", PORT_1);
			return;
		}

		if (n)
		{
			// process received characters
			for (i=0; i<n; ++i)
			{
  				if (write(fd0, &buf[i], 1) == -1)
				{
					printf ("  unable to write to:  %s\n", PORT_0);
					return;
				}
			}
		}
	}
}

int serialOpen (int *fd0, int *fd1)
{
  	*fd0 = open (PORT_0, O_RDWR | O_NOCTTY | O_NDELAY);

	if (*fd0 == -1)
	{
		printf ("  unable to open:  %s\n", PORT_0);
		return (0);
	}

  	*fd1 = open (PORT_1, O_RDWR | O_NOCTTY | O_NDELAY);

	if (*fd1 == -1)
	{
		printf ("  unable to open:  %s\n", PORT_1);
		return (0);
	}

	return (1);
}

void *onExitHandler (int i, void *p)
{
	close(fd0);
	close(fd1);
}

static void daemonMode (void)
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    // Close all open file descriptors
	// (except the open serial ports)
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>0; x--)
    {
		if (x != fd0 && x != fd1)
        	close (x);
    }

    /* Open the log file */
    openlog ("firstdaemon", LOG_PID, LOG_DAEMON);

	on_exit( (void *) &onExitHandler, NULL);

	for (;;)
	{
		serialLoop(fd0, fd1);
        //usleep(500); // sleep for .5 millisecond
	}
}

int main (int argc, char *argv[])
{
	int buf[256];

	int n;

	printf ("justatest\n");

	if (serialOpen(&fd0, &fd1)) daemonMode();
}

// vim:ts=4:autoindent:
