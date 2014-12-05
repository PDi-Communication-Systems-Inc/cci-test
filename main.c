/**************************************************

file: main.c
purpose: test the TV's CCI port, then create 
  passthrough
 
  - writes a CCI command to the TV and 
  validates the response
  - continuously monitors TV and external comm
  ports sending all data from one to the other
 
HISTORY: 
V1.0 - Inital version runs from Android terminal 
 
V1.1 - Designed to run from Android init script after 
       booting.  It will send status info to the TV
       screen, the local terminal, and to logcat.
 
V1.2 - Updated rs232.c with latest open source. 
 
V1.3 - After testing the CCI connection to the TV, 
       enter an infinite loop passing data back and
       forth between the external CCI connection and
       the TV.
 
V1.4 - Changed loop wait period from 100ms to 5ms since 
       an ACK is only 2 bytes being transmitted in 4.2ms. 
 
**************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

#include "rs232.h"

#define  LOG_TAG    "CCI"

#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); printf(__VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__); printf(__VA_ARGS__)

#define DLE 0x10
#define STX 0x02
#define ETX 0x03

static const unsigned char PresenceReplyPacket[] = {DLE, STX,0xD,0,0x0D,DLE, ETX};
static const unsigned char writeOSD_title[] = {DLE, STX, 0x1A, 0x07, 0x00, 0x00, 0x01, 0x01, 0x13, \
    0x54, 0x65, 0x73, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x41, 0x6E, 0x64, 0x72, 0x6F, 0x69, 0x64, 0x20, \
    0x43, 0x43, 0x49, 0x06, 0xE4, DLE, ETX}; // Testing Android CCI
static const unsigned char writeOSD_pass[] = {DLE, STX, 0x1A, 0x00, 0x02, 0x00, 0x03, 0x01, 0x0A, \
    0x20, 0x43, 0x43, 0x49, 0x20, 0x50, 0x41, 0x53, 0x53, 0x20, 0x02, 0x90, DLE, ETX};
static const unsigned char writeOSD_fail[] = {DLE, STX, 0x1A, 0x07, 0x04, 0x01, 0x03, 0x01, 0x0A, \
    0x20, 0x43, 0x43, 0x49, 0x20, 0x46, 0x41, 0x49, 0x4C, 0x20, 0x02, 0x7F, DLE, ETX};
static const unsigned char clearOSD[] = {DLE, STX, 0x19, 0x00, 0x19, DLE, ETX};


// define UART indexes
#define TV_CCI_UART     30
#define EXT_CCI_UART    31



// helper function converts an array of bytes into a string of hex values
void bin2hex(const unsigned char *in, int inlen, char *out) {
    int i;
    char tempstr[4];

    out[0] = 0;
    for (i=0; i < inlen; i++)
    {
        sprintf(tempstr, "%02x ", in[i]);
        strcat(out, tempstr);
    }

    return;
}

int main()
{
    int i, n, counter = 0,
    cport_nr = TV_CCI_UART, 
    bdrate = 4800; 

    unsigned char buf[4096];
    char dbuf[4096];
    int powerOn = 1;
    int m =0;

    LOGI("\nBeginning CCI Test v1.4...\n");

    memset(&buf[0], 0, sizeof(buf));

    if (RS232_OpenComport(cport_nr, bdrate, "8N1"))
    {
        LOGE("ERROR:  Can not open comport ttymxc1\n");

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
    m = RS232_SendBuf(cport_nr, writeOSD_title, sizeof(writeOSD_title));    

    if (m >0)
    {
        LOGI("Sent OSD command.  TV should display 'Testing Android CCI' text.\n");
    } else if (m == -1)
    {
        LOGE("\nERROR:  Comm port error in sending  \n");
    }

    usleep(1000000); // sleep for 1 seconds waiting for response from TV




    n = RS232_PollComport(cport_nr, buf, 4095);

    if (n > 0)
    {
        buf[n] = 0;   /* always put a "null" at the end of a string! */

        // Print out what was received.
        bin2hex(buf, n, dbuf);
        LOGD("Received %i bytes from TV: %s", n, dbuf);

        // Verify a proper ACK is received [10][06]
        if ((buf[0] == 0x10) && (buf[1] == 0x06))
        {
            LOGI("Test Passed\n");
            // send message to TV screen
            m = RS232_SendBuf(cport_nr, writeOSD_pass, sizeof(writeOSD_pass));    
        }
        else
        {    
            LOGI("********** FAIL (incorrect message from TV board) **********\n");
            // send message to TV screen
            m = RS232_SendBuf(cport_nr, writeOSD_fail, sizeof(writeOSD_fail));    
        }
    }
    else 
    {
        LOGI("********** FAIL (no response from TV board) **********\n");
        // send message to TV screen
        m = RS232_SendBuf(cport_nr, writeOSD_fail, sizeof(writeOSD_fail));    
    }

    usleep(8000000); // leave message on screen for 8 seconds

    // clear TV screen
    m = RS232_SendBuf(cport_nr, clearOSD, sizeof(clearOSD));    

    LOGI("...CCI test complete\n\nStarting CCI passthrough.\nCTRL-C to end.\n");


    // ** CCI test is complete, now start the CCI passthrough process
    // ** Infinite loop:  read from ttymxc1 - write to ttymxc3
    // **                 read from ttymxc3 - write to ttymxc1

    // ttymxc1 is already open, now open ttymxc3
    if (RS232_OpenComport(EXT_CCI_UART, 4800, "8N1"))
    {
        LOGE("ERROR:  Can not open comport ttymxc3\n");

        return(0);
    }
    
    while(1)
    {
        // Receive from TV
		n = RS232_PollComport(TV_CCI_UART, buf, 4095);
    
        if (n > 0)
        {
            // Log what was received.
            bin2hex(buf, n, dbuf);
            LOGD("Received %i bytes from TV: %s", n, dbuf);
    
            // send message to external CCI
            m = RS232_SendBuf(EXT_CCI_UART, buf, n);    
        }
    
    
        // Receive from external CCI host
		n = RS232_PollComport(EXT_CCI_UART, buf, 4095);
    
        if (n > 0)
        {
            // Log what was received.
            bin2hex(buf, n, dbuf);
            LOGD("Received %i bytes from external CCI host: %s", n, dbuf);
    
            // send message to TV
            m = RS232_SendBuf(TV_CCI_UART, buf, n);    
        }

        usleep(5000); // pause for 5ms in between polling
    }

    return(0);
}
