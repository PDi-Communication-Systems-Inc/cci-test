/**************************************************

file: main.c
purpose: simple demo that receives characters from
the serial port and print them on the screen

**************************************************/

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

#include "rs232.h"

static const unsigned char PresenceReplyPacket[] = {0x10,2,0xD,0,0x0D,0x10,3};
static const unsigned char setVolumeLouderwithOSD[] = {0x10,2,0x03,0x01,0x01,0x01,00,06,0x10,3};

int main()
{
    int i, n, counter = 0,
    cport_nr=30,        /* /dev/ttyS0 (COM1 on windows) cport_nr=16*/
    bdrate=4800;       /* 9600 baud */

    unsigned char buf[4096];
    int powerOn = 1;
    int m =0;

    printf("\nBeginning CCI test v1.0...\n");

    memset(&buf[0], 0, sizeof(buf));

    if (RS232_OpenComport(cport_nr, bdrate))
    {
        printf("ERROR:  Can not open comport\n");

        return(0);
    }

    //disable RTS
    RS232_disableRTS(cport_nr);

    //enable dtr
    RS232_enableDTR(cport_nr);

    //int RS232_SendBuf(int comport_number, unsigned char *buf, int size)

    //Sends multiple bytes via the serial port. Buf is a pointer to a buffer
    //and size the size of the buffer in bytes.
    //Returns -1 in case of an error, otherwise it returns the amount of bytes sent.
    //This function blocks (it returns after all the bytes have been processed).

    //m = RS232_SendBuf(cport_nr, PresenceReplyPacket, sizeof(PresenceReplyPacket));
    m = RS232_SendBuf(cport_nr, setVolumeLouderwithOSD, sizeof(setVolumeLouderwithOSD));    

    if (m >0)
    {
        printf("Sent volume up command.  TV volume should have increased by 1.\n");
    } else if (m == -1)
    {
        printf("\nERROR:  Comm port error in sending  \n");
    }

    usleep(1000000); // sleep for 1 seconds




    n = RS232_PollComport(cport_nr, buf, 4095);

    if (n > 0)
    {
        buf[n] = 0;   /* always put a "null" at the end of a string! */

        // Print out what was received.
        printf("received %i bytes: ", n);
        for (i=0; i < n; i++)
        {
            printf("%x ",buf[i]);
        }
        printf("\n");

        // Verify a proper ACK is received [10][06]
        if ((buf[0] == 0x10) && (buf[1] == 0x06))
        {
            printf("Pass\n");
        }
        else
        {    
            printf("********** FAIL (incorrect message from TV board) **********\n");
        }
    }
    else 
    {
        printf("Received nothing from TV board.\n********** FAIL (no response from TV board) **********\n");
    }
    printf("...CCI test complete\n\n");

    return(0);
}


