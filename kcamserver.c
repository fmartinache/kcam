#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edtinc.h"             // for EDT PCI board


/* -------------------------------------------------------------------------
 * this program is based on the architecture developed by O.Guyon to drive
 * the  CRED2 camera on SCExAO using an EDT PCIe interface. It is here used
 * to drive a CRED1 camera. Frantz Martinache (June 2018)
 * ------------------------------------------------------------------------- */

#define SERBUFSIZE 512
static char buf[SERBUFSIZE+1];

int verbose = 1;

/* =========================================================================
 *                  Displays a help menu upon request
 * ========================================================================= */
int print_help() {
  char fmt[20] = "%15s %20s %40s\n";
  char line[80] =
	"-----------------------------------------------------------------------------\n";
  printf("%s", "\033[01;34m");
  printf("%s", line);
  printf("               K-Cam, the KERNEL-camera\n");
  printf("%s", line);
  printf(fmt, "command", "parameters", "description");
  printf("%s", line);
  printf(fmt, "help", "", "lists commands");
  printf(fmt, "quit", "", "exit server");
  printf(fmt, "test", "", "serial comm test");
  printf(fmt, "exit", "", "exit server");
  printf("%s", line);
  printf("%s", "\033[00m");
}

/* =========================================================================
 *                  read pdv command line response
 * ========================================================================= */
int readpdvcli(EdtDev *ed, char *outbuf) {
  int ret = 0;
  u_char  lastbyte, waitc;
  int     length=0;
	
  outbuf[0] = '\0';
  do {
    ret = pdv_serial_read(ed, buf, SERBUFSIZE);
    if (verbose)
      printf("read returned %d\n", ret);

    if (*buf)
      lastbyte = (u_char)buf[strlen(buf)-1];

    if (ret != 0) {
      buf[ret + 1] = 0;
      strcat(outbuf, buf);
      length += ret;
    }

    if (ed->devid == PDVFOI_ID)
      ret = pdv_serial_wait(ed, 500, 0);
    else if (pdv_get_waitchar(ed, &waitc) && (lastbyte == waitc))
      ret = 0; /* jump out if waitchar is enabled/received */
    else ret = pdv_serial_wait(ed, 500, 64);
  } while (ret > 0);
}

/* =========================================================================
 *                        generic server command
 * ========================================================================= */
int server_command(EdtDev *ed, const char *cmd) {
  char tmpbuf[SERBUFSIZE];
  sprintf(tmpbuf, "%s", cmd);

  printf("the command is %s\n", tmpbuf);
  return 0;
}

/* =========================================================================
 *                            Main program
 * ========================================================================= */
int main() {
  char prompt[200];
  char cmdstring[200];
  char auxstring[200];
  char str0[20];
  char str1[20];

  int cmdOK = 0;
  EdtDev *ed;
  int unit = 0;
  int channel = 0;
  int baud = 115200;
  int timeout = 0;
  char outbuf[2000];	

  // --------------- initialize data structures -----------------
  
  // -------------- open a handle to the device -----------------
  ed = pdv_open_channel(EDT_INTERFACE, unit, channel);
  if (ed == NULL) {
    pdv_perror(EDT_INTERFACE);
    return -1;
  }

  printf("device name is: %s\n", ed->edt_devname);

  // ---------------- command line interpreter ------------------
  sprintf(prompt, "\033[01;33m%s>\033[00m", "Kcam");
  for (;;) {
    cmdOK = 0;
    printf("%s ", prompt);
    fgets(cmdstring, 200, stdin);
    
    if (cmdOK == 0)
      if (strncmp(cmdstring, "test", strlen("test")) == 0) {
	readpdvcli(ed, outbuf);
	server_command(ed, "coucou");
	printf("outbuf: %s\n", outbuf);
	cmdOK = 1;
      }
    
    if (cmdOK == 0)
      if (strncmp(cmdstring, "rawcmd", strlen("rawcmd")) == 0) {
	sscanf(cmdstring, "%s %s", str0, str1);
	readpdvcli(ed, outbuf);
	server_command(ed, str1);
	printf("outbuf: %s\n", outbuf);
	cmdOK = 1;
      }
    
    if (cmdOK == 0)
      if (strncmp(cmdstring, "help", strlen("help")) == 0) {
	print_help();
	cmdOK = 1;
      }
    
    if (cmdOK == 0)
      if (strncmp(cmdstring, "quit", strlen("quit")) == 0) {
	pdv_close(ed);
	printf("Bye!\n");
	exit(0);
      }
    
    if (cmdOK == 0)
      if (strncmp(cmdstring, "exit", strlen("quit")) == 0) {
	pdv_close(ed);
	printf("Bye!\n");
	exit(0);
      }
    
    if (cmdOK == 0) {
      printf("Unkown command: %s\n", cmdstring);
      print_help();
    }
  }
  
  exit(0);
}

