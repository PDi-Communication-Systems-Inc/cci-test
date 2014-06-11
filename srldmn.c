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

#include <android/log.h>

#define VERIFY_PACKET (0)
//#define DEBUG (0)

#if ((VERIFY_PACKET)==(1))
#define DBG_VRFY_PCKT
#endif

#define STX 0x02
#define ETX 0x03
#define DLE 0x10

#define tn(n) (((argc)>=((n)+1))?(argv[(n)]):("NULL"))

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef signed char S8;
typedef signed short S16;
typedef signed int S32;

int fd0, fd1;
char *p0, *p1;

int b0h, b0t;
int b1h, b1t;

U8 buf0[256]; // circular receive buffer p0
U8 buf1[256]; // circular receive buffer p1

void clrP0 (void)
{
	b0h=0;
	b0t=0;
}

int szP0 (void)
{
	if (b0h >= b0t)
		return (b0h - b0t - 1);
	else
		return (sizeof(buf0) - b0t + b0h);
}

void pshP0 (U8 b)
{
	if ( (int) ( (b0h+1) % sizeof(buf0) ) == b0t ) ++b0t;

	buf0[b0h] = b;

	b0h = ( (b0h+1) % sizeof(buf0) );
}

U8 popP0 (void)
{
	U8 b;

	if (szP0())
	{
		b = buf0[b0t];

		b0t = ( (b0t+1) % sizeof(buf0) );

		return (b);
	}

	return (0);
}

void rcvP0 (int len, U8 *b)
{
	int i;

	for (i=0; i<len; ++i) pshP0(*b++);
}

/*
 * transmit last packet on buffer
 */
void tmtP0 (int len)
{
	int i;
	U8 b;

	while (szP0() > len) popP0();

	for (i=0; i<len; ++i)
	{
		if (!szP0()) break;

		b = popP0();

		write (fd0, &b, 1);
	}
}

void clrP1 (void)
{
	b1h=0;
	b1t=0;
}

int szP1 (void)
{
	if (b1h >= b1t)
		return (b1h - b1t - 1);
	else
		return (sizeof(buf1) - b1t + b1h);
}

void pshP1 (U8 b)
{
	if ( (int) ( (b1h+1) % sizeof(buf1) ) == b1t ) ++b1t;

	buf1[b1h] = b;

	b1h = ( (b1h+1) % sizeof(buf1) );
}

U8 popP1 (void)
{
	U8 b;

	if (szP1())
	{
		b = buf1[b1t];

		b1t = ( (b1t+1) % sizeof(buf1) );

		return (b);
	}

	return (0);
}

void rcvP1 (int len, U8 *b)
{
	int i;

	for (i=0; i<len; ++i) pshP1(*b++);
}

/*
 * transmit last packet on buffer
 */
void tmtP1 (int len)
{
	int i;
	U8 b;

	while (szP1() > len) popP1();

	for (i=0; i<len; ++i)
	{
		if (!szP1()) break;

		b = popP1();

		write (fd1, &b, 1);
	}
}

void serialInit (void)
{
	clrP0();
	clrP1();
}

int serialConfig (int fd)
{
	struct termios t;

	if (tcgetattr(fd, &t) == -1)
	{
		printf ("  unable to get attributes for port:  %i\n", fd);
		return (0);
	}

	t.c_cflag = B4800 | CS8 | CLOCAL | CREAD;
	t.c_iflag = IGNPAR;
	t.c_oflag = 0;
	t.c_lflag = 0;
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSANOW, &t) == -1)
	{
		printf ("  unable to set attributes for port:  %i\n", fd);
		return (0);
	}

	return (1);
}

int serialOpen (char *p0, char *p1)
{
#ifndef DEBUG
	//__android_log_write(ANDROID_LOG_INFO, "starting", "");
	syslog (LOG_INFO, "%s", "starting");
#else
	printf ("serialOpen\n");
	//__android_log_write(ANDROID_LOG_INFO, "starting", "justatest");
#endif

	if (p0 != NULL)
	{
		fd0 = open (p0, O_RDWR | O_NOCTTY | O_NDELAY);
		printf ("  port: %s initialized\n", p0);
	}
	else
	{
		printf ("  Port 0 NULL\n");
		return (0);
	}

	if (fd0 == -1)
	{
		printf ("  unable to open:  %s\n", p0);
		return (0);
	}
	else
	{
#ifndef DEBUG
		syslog (LOG_INFO, "%s: %s", "opened serial port 0", p0);
#else
		printf ("opened serial port 0\n");
#endif

		if (serialConfig(fd0))
		{
			printf ("  serial port p0 configured\n");
		}
		else
		{
			printf ("  unable to config serial:  %s\n", p0);
			return (0);
		}
	}

	if (p1 != NULL)
	{
		fd1 = open (p1, O_RDWR | O_NOCTTY | O_NDELAY);
		printf ("  port: %s initialized\n", p1);
	}
	else
	{
		printf ("  Port 1 NULL\n");
		return (0);
	}

	if (fd1 == -1)
	{
		printf ("  unable to open:  %s\n", p1);
		return (0);
	}
	else
	{
#ifndef DEBUG
		syslog (LOG_INFO, "%s: %s", "opened serial port 1", p1);
#else
		printf ("opened serial port 1\n");
#endif

		if (serialConfig(fd1))
		{
			printf ("  serial port p1 configured\n");
		}
		else
		{
			printf ("  unable to config serial:  %s\n", p1);
			return (0);
		}
	}

	return (1);
}

void serialClose (void)
{
	close (fd0);
	close (fd1);
}

int matchPacket (U8 b)
{
	static int s = 0;	// state
	static int len = 0;	// packet length

	static U8 cs = 0;	// checksum calculated
	static U8 css = 0;	// checksum from packet

	//if (DEBUG) printf("%i,%02X ", s, b);

	switch (s)
	{
		case 0:
			if (b == DLE)
			{
				len = 1;
				s = 1;
			}
			break;
		case 1:
			if (b == STX)
			{
				++len;
				s = 2;
			}
			break;
		case 2: // data
			cs = b;
			css = b;
			++len;
			s = 3;
			break;
		case 3: // data
			if (b != DLE)
			{
				cs += b;
				css = b;
				++len;
			}
			else
			{
				cs -= css;
				++len;
				s = 4;
			}
			break;
		case 4:
			++len;
			if (b == ETX)
			{
				// verify checksum
				if (cs == css)
				{
					//if (DEBUG) printf(" (%02X %02X) match\n", cs, css);
					s = 0;
					return (len);
				}
			}
			else
			{
				cs += DLE;
				cs += b;
				css = b;
				s = 3;
			}
			break;
		default:
			s = 0;
			break;
	}

	return (0);
}

void serialLoop (void)
{
	U8 buf[256];
	int vf=0, n, i, len, tst=0;

#ifndef DEBUG
	syslog (LOG_INFO, "%s", "starting serialLoop");
#else
	printf ("serialLoop\n");
#endif

#ifdef DBG_VRFY_PCKT
	vf=1;
#endif

	for (;;)
	{
		if (!tst)
		{
			printf ("serialLoop: for\n");
			tst=1;
		}

		/*
		 * P0 --> P1
		 */

		if ( (n=read (fd0, &buf[0], sizeof(buf)-1) ) )
		{
			if (vf) rcvP0 (n, &(buf[0]));

			for (i=0; i<n; ++i)
			{
#ifndef DEBUG
				syslog (LOG_INFO, "p0: rcv: %02X ", buf[i]);
#else
				printf ("p0: rcv: %02X ", buf[i]);
#endif

				if (vf)
				{
					if ( (len=matchPacket(buf[i]) ) ) tmtP1(len);
				}
				else
				{
					write (fd1, &buf[i], 1);
#ifndef DEBUG
					syslog (LOG_INFO, "p1: tmt: %02X ", buf[i]);
#else
					printf ("p1: tmt: %02X ", buf[i]);
#endif
				}
			}
		}
		else if (n == -1)
		{
			printf ("  unable to read from:  %s\n", p0);
			return;
		}

		/*
		 * P1 --> P0
		 */

		if ( (n=read (fd1, &buf[0], sizeof(buf)-1) ) )
		{
			if (vf) rcvP1 (n, &(buf[0]));

			for (i=0; i<n; ++i)
			{
#ifndef DEBUG
				syslog (LOG_INFO, "p1: rcv: %02X ", buf[i]);
#else
				printf ("p1: rcv: %02X ", buf[i]);
#endif

				if (vf)
				{
					if ( (len=matchPacket(buf[i]) ) ) tmtP0(len);
				}
				else
				{
					write (fd0, &buf[i], 1);
#ifndef DEBUG
					syslog (LOG_INFO, "p0: tmt: %02X ", buf[i]);
#else
					printf ("p0: tmt: %02X ", buf[i]);
#endif
				}
			}
		}
		else if (n == -1)
		{
			printf ("  unable to read from:  %s\n", p1);
			return;
		}
	}
}
/*
void *onExitHandler (int i, void *p)
{
	close (fd0);
	close (fd1);
}
*/
void daemonMode (void)
{
	int x;
	pid_t pid;

	pid = fork();

	if (pid < 0) exit (EXIT_FAILURE);
	if (pid > 0) exit (EXIT_SUCCESS);

	if (setsid() < 0) exit (EXIT_FAILURE);

	signal (SIGCHLD, SIG_IGN);
	signal (SIGHUP, SIG_IGN);

	pid = fork();

	if (pid < 0) exit (EXIT_FAILURE);
	if (pid > 0) exit (EXIT_SUCCESS);

	umask(0);
	chdir("/");

	for (x=sysconf(_SC_OPEN_MAX); x>0; x--)
	{
		if (x != fd0 && x != fd1)
			close (x);
	}

	//on_exit ( (void *) &onExitHandler, NULL);

	for (;;)
	{
		serialLoop();
		usleep(250);
	}
}

void tst (int argc, char *argv[])
{
	int vf=0;

#ifdef DBG_VRFY_PCKT
	vf=1;
#endif

	printf ("justatest\n");

	printf ("argv[0] = %s\n", tn(0));
	printf ("argv[1] = %s\n", tn(1));
	printf ("argv[2] = %s\n", tn(2));

	//printf ("  vf = %i\n", vf);
	//exit (0);
}

int main (int argc, char *argv[])
{
	int n;
	U8 buf[256];

	p0 = argv[1];
	p1 = argv[2];

	tst (argc, argv);

	serialInit(); // afaik

#ifndef DEBUG
	if (serialOpen(argv[1], argv[2])) daemonMode();
#else
	printf ("justatest\n");

	serialOpen(argv[1], argv[2]);

	serialLoop();

	serialClose();
#endif

	return (0);
}

// vim:ts=4:autoindent:
