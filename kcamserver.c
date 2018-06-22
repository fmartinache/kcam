#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edtinc.h"             // for EDT PCI board
#include "cred1struct.h"        // CREDSTRUCT data structure

/* -------------------------------------------------------------------------
 * this program is based on the architecture developed by O.Guyon to drive
 * the  CRED2 camera on SCExAO using an EDT PCIe interface. It is here used
 * to drive a CRED1 camera. Frantz Martinache (June 2018)
 * ------------------------------------------------------------------------- */

#define SERBUFSIZE 512
#define CMDBUFSIZE 200
static char buf[SERBUFSIZE+1];

int verbose = 1;

CRED1STRUCT *kcamconf;

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
  printf(fmt, "help",  "", "lists commands");
  printf(fmt, "quit",  "", "exit server");
  printf(fmt, "stop",  "", "stop the acquisition");
  printf(fmt, "start", "", "start the acquisition (inf. loop)");
  printf(fmt, "exit",  "", "exit server");
  printf(fmt, "RAW",   "CRED1 command", "serial comm test (expert mode)");
  printf("%s", line);
  printf("%s", "\033[00m");
}

/* =========================================================================
 *                  read pdv command line response
 * ========================================================================= */
int readpdvcli(EdtDev *ed, char *outbuf) {
  int     ret = 0;
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
  char outbuf[2000];

  readpdvcli(ed, outbuf); // flush
  sprintf(tmpbuf, "%s\r", cmd);
  pdv_serial_command(ed, tmpbuf);
  if (verbose)
    printf("command: %s", tmpbuf);
  return 0;
}

/* =========================================================================
 *                    generic server query (expects float)
 * ========================================================================= */
float server_query_float(EdtDev *ed, const char *cmd) {
  char outbuf[2000];
  float fval;

  server_command(ed, cmd);
  readpdvcli(ed, outbuf);
  sscanf(outbuf, "%f", &fval);

  return fval;
}

/* =========================================================================
 *                            Main program
 * ========================================================================= */
int main() {
  char prompt[CMDBUFSIZE];
  char cmdstring[CMDBUFSIZE];
  char serialcmd[CMDBUFSIZE];
  char str0[20];
  char str1[20];
  char *copy;
  char *token;
  
  int cmdOK = 0;
  EdtDev *ed;
  int unit = 0; // 1 for kcam
  int chn = 0;
  int baud = 115200;
  int timeout = 0;
  char outbuf[2000];
  float fval = 0.0; // float value return
  float ival = 0; // integer value return

  printf("%s", "\033[01;32m");
  printf("--------------------------------------------------------\n");
  printf(" _                                                      \n");
  printf("| | _____ __ _ _ __ ___    ___  ___ _ ____   _____ _ __ \n");
  printf("| |/ / __/ _` | '_ ` _ \\  / __|/ _ \\ '__\\ \\ / / _ \\ '__|\n");
  printf("|   < (_| (_| | | | | | | \\__ \\  __/ |   \\ V /  __/ |   \n");
  printf("|_|\\_\\___\\__,_|_| |_| |_| |___/\\___|_|    \\_/ \\___|_|   \n");
  printf("\n");
  printf("--------------------------------------------------------\n");
  printf("%s", "\033[00m");

  // --------------- initialize data structures -----------------
  initCRED1STRUCT();
  kcamconf[chn].row0 = 0;
  kcamconf[chn].row1 = 256;
  kcamconf[chn].col0 = 0;
  kcamconf[chn].col1 = 10;

  printCRED1STRUCT(0);

  // -------------- open a handle to the device -----------------
  ed = pdv_open_channel(EDT_INTERFACE, chn, chn);
  if (ed == NULL) {
    pdv_perror(EDT_INTERFACE);
    return -1;
  }
  printf("device name is: %s\n", ed->edt_devname);
  pdv_set_baud(ed, baud);
  printf("serial timeout: %d\n", ed->dd_p->serial_timeout);

  // ---------------- command line interpreter ------------------
  sprintf(prompt, "\033[01;33m%s>\033[00m", "Kcam");

  for (;;) {
    cmdOK = 0;
    printf("%s ", prompt);
    fgets(cmdstring, CMDBUFSIZE, stdin);
    
    // ------------------------------------------------------------------------
    //                        ACQUISITION CONTROL
    // ------------------------------------------------------------------------
    if (cmdOK == 0)
      if (strncmp(cmdstring, "stop", strlen("stop")) == 0) {
	system("tmux send-keys -t kcamrun 'echo STOP acquire' C-m");
	//system("tmux send-keys -t kcamrun C-c");
	cmdOK = 1;
      }

    if (cmdOK == 0)
      if (strncmp(cmdstring, "start", strlen("start")) == 0) {
	//system("tmux send-keys -t kcamrun \"./kcam_acquire -u 1 -l 0\" C-m");
	system("tmux send-keys -t kcamrun 'echo START acquire' C-m");
	cmdOK = 1;
      }

    // ------- cropping on ? ----------
    // ------- cropping config --------

    // ------------------------------------------------------------------------
    //                          READOUT MODE
    // ------------------------------------------------------------------------

    if (cmdOK == 0) // ------- get readout mode --------
      if (strncmp(cmdstring, "gmode", strlen("gmode")) == 0) {
	sprintf(serialcmd, "mode raw");
	server_command(ed, serialcmd);
	readpdvcli(ed, outbuf);
	sprintf(kcamconf[0].readmode, "%s", outbuf);
	printf("readmode: \033[01;31m%s\033[00m\n", kcamconf[chn].readmode);
	cmdOK = 1;
      }

    if (cmdOK == 0) // ------- set readout mode --------
      if (strncmp(cmdstring, "smode", strlen("smode")) == 0) {
	sscanf(cmdstring, "%s %s", str0, str1);

	// is requested readout mode valid?
	if (strncmp(str1, "globalresetsingle", strlen("globalresetsingle")) == 0) {
	  sprintf(serialcmd, "set mode %s", "globalresetsingle");
	  //system("tmux send-keys -t kcamrun C-c");
	  //server_command(ed, serialcmd);
	  //system("tmux send-keys -t kcamrun \"./kcam_acquire -u 1 -l 0\" C-m");
	  printf("%s\n", serialcmd); // temp. information
	  sprintf(kcamconf[chn].readmode, "global_single");
	}

	else if (strncmp(str1, "globalresetcds", strlen("globalresetcds")) == 0) {
	  sprintf(serialcmd, "set mode %s", "globalresetscds");
	  //system("tmux send-keys -t kcamrun C-c");
	  //server_command(ed, serialcmd);
	  //system("tmux send-keys -t kcamrun \"./kcam_acquire -u 1 -l 0\" C-m");
	  printf("%s\n", serialcmd); // temp. information
	  sprintf(kcamconf[chn].readmode, "global_cds");
	}

	else if (strncmp(str1, "rollingresetcds", strlen("rollingresetcds")) == 0) {
	  sprintf(serialcmd, "set mode %s", "rollingresetscds");
	  //system("tmux send-keys -t kcamrun C-c");
	  //server_command(ed, serialcmd);
	  //system("tmux send-keys -t kcamrun \"./kcam_acquire -u 1 -l 0\" C-m");
	  printf("%s\n", serialcmd); // temp. information
	  sprintf(kcamconf[chn].readmode, "global_cds");
	}

	else {
	  printf("readmode: \033[01;31m%s\033[00m is not possible\n", str1);
	}
	cmdOK = 1;
      }

    // ------------------------------------------------------------------------
    //                          DETECTOR GAIN
    // ------------------------------------------------------------------------

    if (cmdOK == 0) // ------- get gain --------
      if (strncmp(cmdstring, "ggain", strlen("ggain")) == 0) {
	sprintf(serialcmd, "gain raw");
	kcamconf[0].gain = server_query_float(ed, serialcmd);
	printf("max fps: \033[01;31m%f\033[00m\n", kcamconf[chn].gain);
	cmdOK = 1;
      }

    if (cmdOK == 0) // ------- set gain --------
      if (strncmp(cmdstring, "sgain", strlen("sgain")) == 0) {
	sscanf(cmdstring, "%s %f", str0, &fval);
	sprintf(serialcmd, "set fps %f", fval);
	server_command(ed, serialcmd);
	kcamconf[chn].gain = fval;
	cmdOK = 1;
      }

    // ------------------------------------------------------------------------
    //                            EXPOSURE TIME
    // ------------------------------------------------------------------------

    if (cmdOK == 0) // ------- get exposure time --------
      if (strncmp(cmdstring, "gtint", strlen("gtint")) == 0) {
	sprintf(serialcmd, "fps raw");
	kcamconf[0].fps = server_query_float(ed, serialcmd);
	fval = 1000.0/fval; // from FPS to exposure time
	printf("exposure time is: \033[01;31m%f\033[00m\n", fval);
	kcamconf[0].tint = fval;
	cmdOK = 1;
      }
    
    if (cmdOK == 0) // ------- set exposure time --------
      if (strncmp(cmdstring, "stint", strlen("stint")) == 0) {
	sscanf(cmdstring, "%s %f", str0, &fval);
	fval = 1000.0 / fval; // exp. time -> fps
	if (fval > kcamconf[chn].maxFps)
	  fval = kcamconf[chn].maxFps;
	kcamconf[chn].tint = 1000.0 / fval; // -> exp. time

	sprintf(serialcmd, "set fps %f", fval);
	server_command(ed, serialcmd);
	cmdOK = 1;
	}
    
    // ------------------------------------------------------------------------
    //                            FRAME RATE
    // ------------------------------------------------------------------------

    if (cmdOK == 0) // ------- get max fps --------
      if (strncmp(cmdstring, "gfpsmax", strlen("gfpsmax")) == 0) {
	sprintf(serialcmd, "maxfps raw");
	kcamconf[0].maxFps = server_query_float(ed, serialcmd);
	printf("max fps: \033[01;31m%f\033[00m\n", kcamconf[0].maxFps);
	cmdOK = 1;
      }

    if (cmdOK == 0) // -------- get frame rate ---------
      if (strncmp(cmdstring, "gfps", strlen("gfps")) == 0) {
	sprintf(serialcmd, "fps raw");
	kcamconf[0].fps = server_query_float(ed, serialcmd);
	printf("fps: \033[01;31m%f\033[00m\n", kcamconf[0].fps);
	cmdOK = 1;
      }

    if (cmdOK == 0) // -------- set frame rate ---------
      if (strncmp(cmdstring, "sfps", strlen("sfps")) == 0) {
	sscanf(cmdstring, "%s %f", str0, &fval);
	if (fval > kcamconf[chn].maxFps)
	  fval = kcamconf[chn].maxFps;
	sprintf(serialcmd, "set fps %f", fval);
	server_command(ed, serialcmd);
	readpdvcli(ed, outbuf);
	sscanf(outbuf, "%f", &fval);
	printf("fps: \033[01;31m%f\033[00m\n", fval);
	kcamconf[0].fps = fval;
	cmdOK = 1;
      }

    // ------------------------------------------------------------------------
    //                         CONVENIENCE TOOLS
    // ------------------------------------------------------------------------
    if (cmdOK == 0)
      if (strncmp(cmdstring, "RAW", strlen("RAW")) == 0) {
	copy = strdup(cmdstring);
	token = strsep(&copy, " ");
	server_command(ed, copy);
	readpdvcli(ed, outbuf);
	printf("outbuf: \033[01;31m%s\033[00m\n", outbuf);
	cmdOK = 1;
      }
    
    if (cmdOK == 0)
      if (strncmp(cmdstring, "readconf", strlen("readconf")) == 0) {
	printCRED1STRUCT(0);
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
      if (strncmp(cmdstring, "exit", strlen("exit")) == 0) {
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

