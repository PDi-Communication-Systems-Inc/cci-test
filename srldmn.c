#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

//#define DEBUG (0)
#define NMBR_PORTS (2)

#define STX 0x02
#define ETX 0x03
#define ACK 0x06
#define DLE 0x10

#define tn(n) (((argc)>=((n)+1))?(argv[(n)]):("NULL"))

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef signed char S8;
typedef signed short S16;
typedef signed int S32;

int bAnd, bDbg;
int vrfyPcktFlag;

/*
 * serial port info
 */
struct buffer_t
{
	int fd;			// file descriptor
	char pn[256];	// part name
	int b0t;		// buffer tail
	int b0h;		// buffer head
	U8 b[256];		// buffer
};

struct buffer_t p[NMBR_PORTS];

void clrP (struct buffer_t *p)
{
	p->b0h=0;
	p->b0t=0;
}

int szP (struct buffer_t *p)
{
	if (p->b0h >= p->b0t)
		return (p->b0h - p->b0t - 1);
	else
		return (sizeof(p->b) - p->b0t + p->b0h);
}

void pshP (struct buffer_t *p, U8 b)
{
	if ( ( (p->b0h+1) % (int) sizeof(p->b) ) == p->b0t ) ++p->b0t;

	p->b[p->b0h] = b;

	p->b0h = ( (p->b0h+1) % sizeof(p->b) );
}

U8 popP (struct buffer_t *p, int n)
{
	int i;
	U8 b;

	if (n >= 2)
		for (i=0; i<n-1; ++i) 
			p->b0t = ( (p->b0t+1) % sizeof(p->b) );

	if (szP(p))
	{
		b = p->b[p->b0t];

		p->b0t = ( (p->b0t+1) % sizeof(p->b) );

		return (b);
	}

	return (0);
}

void rcvP (struct buffer_t *p, int len, U8 *b)
{
	int i;

	for (i=0; i<len; ++i) pshP(p, *b++);
}

void tmtP (struct buffer_t *p, int len)
{
	int i;

	if (szP(p) < len || len > (int) sizeof(p->b)) return;

	popP(p, szP(p) - len);

	for (i=0; i<len; ++i) p->b[i] = popP(p, 1);

	// transmit entire packet at once
	write (p->fd, &p->b[0], len);
}

//void serialLog (const char *s, const char *f, ...)
void serialLog (const char *f, ...)
{
	char s[] = "srldmn";

	va_list a;
	va_start(a, f);

	if (bDbg)
		printf (f, a);
	else if (bAnd)
	{
		//__android_log_write (ANDROID_LOG_INFO, f, a);
		//__android_log_vprint (ANDROID_LOG_INFO, s, f, a);
	}
	else
		syslog (LOG_INFO, f, a);

	va_end(a);
}

void serialInit (void)
{
	int i;

	for (i=0; i<NMBR_PORTS; ++i) clrP (&p[i]);
}

int serialConfig (int fd)
{
	struct termios t;

	if (tcgetattr(fd, &t) == -1)
	{
		serialLog ("  unable to get attributes for port:  %i\n", fd);
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
		serialLog ("  unable to set attributes for port:  %i\n", fd);
		return (1);
	}

	return (0);
}

int serialOpenPort (struct buffer_t *p)
{
	if (p != NULL)
	{
		p->fd = open (p->pn, O_RDWR | O_NOCTTY | O_NDELAY);
		serialLog ("  port %s initialized\n", p->pn);
	}
	else
	{
		serialLog ("  port NULL\n");
		return (1);
	}

	if (p->fd == -1)
	{
		serialLog ("  unable to open: %s\n", p->pn);
		return (1);
	}
	else
	{
		serialLog ("opened serial port: %s\n", p->pn);

		if (serialConfig(p->fd))
		{
			serialLog ("  serial port %s configured\n", p->pn);
		}
		else
		{
			serialLog ("  unable to config port: %s\n", p->pn);
			return (1);
		}
	}

	return (0);
}

int serialOpen (void)
{
	int i;

	serialLog ("serialOpen\n");

	for (i=0; i<NMBR_PORTS; ++i)
		if (serialOpenPort (&p[i])) return (1);

	return (0);
}

void serialClose (void)
{
	int i;

	for (i=0; i<NMBR_PORTS; ++i) close (p[i].fd);
}

/*
 * improved packet matcher
 */
int matchPacket (U8 b)
{
	static int s = 0;	// state
	static int len = 0;	// packet length
	static U8 cs = 0;	// checksum calculated

	printf("%i,%02X ", s, b);

	switch (s)
	{
		case 0: // start of packet
			if (b==DLE)
			{
				len=1;
				s=1;
			}
			break;
		case 1: // start of packet
			if (b==STX)
			{
				++len;
				s=2;
			}
			else if (b==ACK)
			{
				++len;
				s=0;
				printf(" match (%i)\n", len);
				return (len);
			}
			else
				s=0;
			break;
		case 2: // start data
			cs=b;
			++len;
			s=3;
			break;
		case 3: // data
			if (cs==b)
			{
				printf ("<");
				s=4;
			}
			cs += b;
			++len;
			break;
		case 4: // end of packet
			++len;
			if (b==DLE)
				s=5;
			else
			{
				if (cs != b) s=3;
				cs += b;
			}
			break;
		case 5: // end of packet
			++len;
			if (b==ETX)
			{
				printf(" match (%i)\n", len);
				s=0;
				return (len);
			}
			if (cs==b)
				s=4;
			else
				s=3;
			cs += b;
			break;
		default:
			s=0;
			break;
	}

	return (0);
}

/*
 * transfer data between the specified serial ports
 */
void serialPort (struct buffer_t *pa, struct buffer_t *pb)
{
	U8 buf[256];

	int len;
	int n, i;
	int vf=vrfyPcktFlag;

	/*
	 * source port pa
	 */
	if ( (n=read (pa->fd, &pa->b[0], sizeof(pa->b)-1) ) == -1 )
	{
		serialLog ("  unable to read from: %s\n", &pa->pn[0]);
		return;
	}
	else if (n)
	{
		if (vf) rcvP (pa, n, &pa->b[0]);

		for (i=0; i<n; ++i)
		{
			serialLog ("%s: rcv: %02X ", &pa->pn[0], &pa->b[i]);

			if (vf)
			{
				if ( (len=matchPacket(buf[i]) ) ) tmtP (pa, len);
			}
			else
			{
				/*
				 * destination port pb
				 */
				write (pb->fd, &pb->b[i], 1);

				serialLog ("%s: rcv: %02X ", &pb->pn[0], &pb->b[i]);
			}
		}
	}
}

void serialLoop (void)
{
	int tst=0;

	serialLog ("serialLoop\n");

	for (;;)
	{
		if (!tst)
		{
			serialLog ("serialLoop: for\n");
			tst=1;
		}

		serialPort (&p[0], &p[1]);
		serialPort (&p[1], &p[0]);
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
	int i, pf;
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

	// close the open descriptors that are not serial ports
	for (x=sysconf(_SC_OPEN_MAX); x; --x)
	{
		pf=1;

		// is the current descriptor a serial port?
		for (i=0; i<NMBR_PORTS; ++i)
			if (x == p[i].fd)
			{
				pf=0;
				break;
			}

		if (pf) close (x);
	}

	//on_exit ( (void *) &onExitHandler, NULL);

	serialLoop();
}

void tstPcktN (int i, U8 *p)
{
	int n;

	for (n=0; n<i; ++n) matchPacket(*(p+n));
}

void tstPckt (void)
{
	// tst
	U8 e0[] = { 0x10, 0x02, 0x01, 0x02, 0x03, 0x06, 0x10, 0x03 }; // valid packt
	U8 e1[] = { 0x10, 0x02, 0x01, 0x02, 0x10, 0x13, 0x10, 0x03 }; // valid packt
	U8 e2[] = { 0x10, 0x02, 0x01, 0x02, 0x11, 0x13, 0x10, 0x03 }; // cs
	U8 e3[] = { 0x10, 0x06 }; // valid ack
	U8 e4[] = { 0x10, 0x06, 0x01, 0x02, 0x03, 0x10, 0x03 }; // valid ack

	tstPcktN (sizeof(e0), &e0[0]);
	tstPcktN (sizeof(e1), &e1[0]);
	//tstPcktN (sizeof(e2), &e2[0]);
	tstPcktN (sizeof(e3), &e3[0]);
	tstPcktN (sizeof(e4), &e4[0]);
}

void showCmdLn (int argc, char *argv[])
{
#ifdef __ANDROID__
	printf ("__ANDROID__\n");
#endif

	serialLog ("justatest\n");

	serialLog ("     argv[0] = %s\n", argv[0]);
	serialLog ("     argv[1] = %s\n", argv[1]);
	serialLog ("     argv[2] = %s\n", argv[2]);

	serialLog ("vrfyPcktFlag = %i\n", vrfyPcktFlag);
	//exit (0);
}

void configVars (int argc, char *argv[])
{
	int i;

#ifdef __ANDROID__
	bAnd=1;
#else
	bAnd=0;
#endif
#ifdef DEBUG
	bDbg=1;
#else
	bDbg=0;
#endif

	if (argv[3] != NULL)
		vrfyPcktFlag=1;
	else
		vrfyPcktFlag=0;

	// save the port name
	for (i=0; i<NMBR_PORTS; ++i)
		strncpy (&p[i].pn[0], argv[i+1], sizeof(p[i].pn)-1);
}

/*
 * ./srlmn <port 0> <port 1> <verify packet>
 */
int main (int argc, char *argv[])
{
	//tstPckt();

	serialLog ("justatest\n");
	showCmdLn (argc, argv);
	configVars(argc, argv);

	serialInit();

	if (bDbg)
	{
		if (serialOpen()) daemonMode();
	}
	else
	{
		serialOpen();
		serialLoop();
		serialClose();
	}

	return (0);
}

// vim:ts=4:autoindent:
