#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
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

int verbose = 0;

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
  printf(fmt, "status", "",        "ready, isbeingcooled, standby, ...");
  printf(fmt, "start", "",         "start the acquisition (inf. loop)");
  printf(fmt, "stop",  "",         "stop the acquisition");
  printf(fmt, "shutdown", "",      "stops the camera!");
  printf("%s", line);
  printf(fmt, "tags", "",          "turns image tagging on/off");
  printf(fmt, "gmode", "",         "get current camera readout mode");
  printf(fmt, "smode", "readmode", "set camera readout mode");
  printf(fmt, "ggain", "",         "get detector gain");
  printf(fmt, "sgain", "[gain]",   "set detector gain");
  printf(fmt, "gtint", "",         "get exposure time (u-second)");
  printf(fmt, "stint", "[tint]",   "set exposure time (u-second)");
  printf(fmt, "gtemp", "",         "get cryo temperature");
  printf(fmt, "gfpsmax", "",       "get max fps available (Hz)");
  printf(fmt, "gfps", "",          "get current fps (Hz)");
  printf(fmt, "sfps", "[fps]",     "set new fps (Hz)");
  //printf(fmt, "", "", "");
  printf("%s", line);
  printf(fmt, "help",  "", "lists commands");
  printf(fmt, "quit",  "", "exit server");
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
  usleep(100000);
  readpdvcli(ed, outbuf);
  sscanf(outbuf, "%f", &fval);

  return fval;
}

/* =========================================================================
 *                 log server interaction in a file
 * ========================================================================= */

// for now, this is just a print screen. But eventually, will log actions
// with time-stamps into a file

int log_action(char *msg) {
  printf("%s\n", msg);
}

/* =========================================================================
 *       send command to the fetcher (shorthand for more concise code)
 * ========================================================================= */
int go_fetcher(char *msg) {
  char cmd[256];
  sprintf("tmux send-keys -t kcam_fetcher %s", msg)
  system(cmd);
}

/* =========================================================================
 *                            Main program
 * ========================================================================= */
int main() {
  char prompt[CMDBUFSIZE];
  char cmdstring[CMDBUFSIZE];
  char serialcmd[CMDBUFSIZE];
  char loginfo[CMDBUFSIZE];
  
  char str0[20];
  char str1[20];
  char *copy;
  char *token;
  
  int cmdOK = 0;
  EdtDev *ed;
  int unit = 1; // 1 for kcam
  int chn = 0;
  int baud = 115200;
  int timeout = 0;
  char outbuf[2000];
  float fval = 0.0; // float value return
  float ival = 0; // integer value return
  int acq_is_on = 0;
  int acq_was_on = 0;
  int tagging = 0;
  int row0, row1, col0, col1; // 

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
  ed = pdv_open_channel(EDT_INTERFACE, unit, chn);
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
    // ------------------------------------------------------------------------
    if (cmdOK == 0) // -------- STATUS ---------
      if (strncmp(cmdstring, "status", strlen("status")) == 0) {
		sprintf(serialcmd, "status raw");
		server_command(ed, serialcmd);
		readpdvcli(ed, outbuf);
		sscanf(outbuf, "%s", str0);
		printf("status: \033[01;31m%s\033[00m\n", str0);
		cmdOK = 1;
      }
	
    // ------------------------------------------------------------------------
    //                        ACQUISITION CONTROL
    // ------------------------------------------------------------------------
    if (cmdOK == 0)
      if (strncmp(cmdstring, "stop", strlen("stop")) == 0) {
		if (acq_is_on == 1) {
		  go_fetcher("C-c");
		  //system("tmux send-keys -t kcam_fetcher C-c");
		  acq_is_on = 0;
		  sprintf(loginfo, "%s", "image fetching interrupted");
		  log_action(loginfo);
		}
		cmdOK = 1;
      }
	
    if (cmdOK == 0)
      if (strncmp(cmdstring, "take5", strlen("take5")) == 0) {
		if (acq_is_on == 0) {
		  go_fetcher("\"./kcamfetch -u 1 -l 5\" C-m");
		  //system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 5\" C-m");
		  sprintf(loginfo, "%s", "fetching 5 images");
		  log_action(loginfo);
		}
		cmdOK = 1;
      }
	
    if (cmdOK == 0)
      if (strncmp(cmdstring, "start", strlen("start")) == 0) {
		if (acq_is_on == 0) {
		  system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
		  sprintf(loginfo, "%s", "start fetching images");
		  log_action(loginfo);
		  acq_is_on = 1;
		}
		cmdOK = 1;
      }
	
    // ------------------------------------------------------------------------
    //                          CROPPING MODE
    // ------------------------------------------------------------------------
    if (cmdOK == 0) // ------- get cropping mode --------
      if (strncmp(cmdstring, "gcrop", strlen("gcrop")) == 0) {
		sprintf(serialcmd, "cropping rows");
		server_command(ed, serialcmd);
		readpdvcli(ed, outbuf);
		sscanf(outbuf, "rows: %d-%d", &row0, &row1);
		
		sprintf(serialcmd, "cropping columns");
		server_command(ed, serialcmd);
		readpdvcli(ed, outbuf);
		sscanf(outbuf, "columns: %d-%d", &col0, &col1);
		
		kcamconf[chn].row0 = row0;
		kcamconf[chn].row1 = row1;
		kcamconf[chn].col0 = col0;
		kcamconf[chn].col1 = col1;
		
		printf("cropmode: \033[01;31mCOLS:%d-%d\033[00m\n", col0, col1);
		printf("cropmode: \033[01;31mROWS:%d-%d\033[00m\n", row0, row1);
		cmdOK = 1;
      }
	
	
    if (cmdOK == 0) // ------- set columns --------
      if (strncmp(cmdstring, "scropcol", strlen("scropcol")) == 0) {
		sscanf(cmdstring, "%s %d-%d", str0, &col0, &col1);
		
		if ((0 <= col0) && (col0 <= 10) && (0 <= col1) && 
			(col1 <= 10) && (col0 < col1)) {
		  
		  sprintf(serialcmd, "set cropping columns %d-%d", col0, col1);
		  if (acq_is_on == 1) {
			go_fetcher("C-c");
			//system("tmux send-keys -t kcam_fetcher C-c");
			acq_was_on = 1;
		  }
		  server_command(ed, serialcmd);
		  kcamconf[chn].col0 = col0;
		  kcamconf[chn].col1 = col1;
		  printCRED1STRUCT(0);
		  
		  if (acq_was_on == 1){
			system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
			acq_was_on = 0;
		  }
		  sprintf(loginfo, "%s", serialcmd);
		  log_action(loginfo);
		}
		else {
		  printf("Not a valid combination.\n");
		}
		cmdOK = 1;
      }
	
    if (cmdOK == 0) // ------- set rows --------
      if (strncmp(cmdstring, "scroprow", strlen("scroprow")) == 0) {
		sscanf(cmdstring, "%s %d-%d", str0, &row0, &row1);
		
		if ((0 < row0) && (row0 <= 256) && (0 < row1) && 
			(row1 <= 256) && (row0 < row1)) {
		  
		  sprintf(serialcmd, "set cropping rows %d-%d", row0, row1);
		  if (acq_is_on == 1) {
			go_fetcher("C-c");
			//system("tmux send-keys -t kcam_fetcher C-c");
			acq_was_on = 1;
		  }
		  server_command(ed, serialcmd);
		  kcamconf[chn].row0 = row0;
		  kcamconf[chn].row1 = row1;
		  printCRED1STRUCT(0);
		  
		  if (acq_was_on == 1){
			system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
			acq_was_on = 0;
		  }
		  sprintf(loginfo, "%s", serialcmd);
		  log_action(loginfo);
		}
		else {
		  printf("Not a valid combination.\n");
		}
		cmdOK = 1;
      }
	
    if (cmdOK == 0) // ------- set crop on/off --------
      if (strncmp(cmdstring, "scrop ", strlen("scrop ")) == 0) {
		sscanf(cmdstring, "%s %s", str0, str1);
		sprintf(serialcmd, "set cropping %s", str1);
		if (acq_is_on == 1) {
		  go_fetcher("C-c");
		  //system("tmux send-keys -t kcam_fetcher C-c");
		  acq_was_on = 1;
		}
		server_command(ed, serialcmd);
		
		if (acq_was_on == 1){
		  system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
		  acq_was_on = 0;
		}
		sprintf(loginfo, "%s", serialcmd);
		log_action(loginfo);
		cmdOK = 1;
      }
	
    // ------------------------------------------------------------------------
    //                            READOUT MODE
    // ------------------------------------------------------------------------
	
    if (cmdOK == 0) // ------- frame tagging --------
      if (strncmp(cmdstring, "tags", strlen("tags")) == 0) {
		if (tagging == 0) {
		  sprintf(serialcmd, "set imagetags on");
		  server_command(ed, serialcmd);
		  tagging = 1;
		  printf("tagging was turned on\n");
		}
		else {
		  sprintf(serialcmd, "set imagetags off");
		  server_command(ed, serialcmd);
		  tagging = 0;
		  printf("tagging was turned off\n");
		}
		cmdOK = 1;
      }
	
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
		  if (acq_is_on == 1) {
			go_fetcher("C-c");
			//system("tmux send-keys -t kcam_fetcher C-c");
			acq_was_on = 1;
		  }
		  server_command(ed, serialcmd);
		  sprintf(kcamconf[chn].readmode, "global_sng");

		  if (acq_was_on == 1){
			system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
			acq_was_on = 0;
		  }
		  sprintf(loginfo, "%s", serialcmd);
		  log_action(loginfo);
		}
	
		// ---------------
		// ---------------

		else if (strncmp(str1, "globalresetcds", strlen("globalresetcds")) == 0) {
		  sprintf(serialcmd, "set mode %s", "globalresetcds");
		  if (acq_is_on == 1) {
			go_fetcher("C-c");
			//system("tmux send-keys -t kcam_fetcher C-c");
			acq_was_on = 1;
		  }
		  server_command(ed, serialcmd);
		  sprintf(kcamconf[chn].readmode, "global_cds");
		  
		  if (acq_was_on == 1) {
			system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
			acq_was_on = 0;
		  }
		  sprintf(loginfo, "%s", serialcmd);
		  log_action(loginfo);
		}
		
		// ---------------
		// ---------------
		
		else if (strncmp(str1, "globalresetnro", strlen("globalresetnro")) == 0) {
		  sprintf(serialcmd, "set mode %s", "globalresetbursts");
		  if (acq_is_on == 1) {
			go_fetcher("C-c");
			//system("tmux send-keys -t kcam_fetcher C-c");
			acq_was_on = 1;
		  }
		  server_command(ed, serialcmd);
		  sprintf(kcamconf[chn].readmode, "global_nro");
		  
		  sprintf(serialcmd, "set imagetags on");
		  server_command(ed, serialcmd);
		  tagging = 1;
		  printf("Image tagging was turned on\n");
		  
		  if (acq_was_on == 1) {
			system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
			acq_was_on = 0;
		  }
		  sprintf(loginfo, "%s", serialcmd);
		  log_action(loginfo);
		}
		
		// ---------------
		// ---------------
		
		else if (strncmp(str1, "rollingresetsingle", strlen("rollingresetsingle")) == 0) {
		  sprintf(serialcmd, "set mode %s", "rollingresetsingle");
		  if (acq_is_on == 1) {
			go_fetcher("C-c");
			//system("tmux send-keys -t kcam_fetcher C-c");
			acq_was_on = 1;
		  }
		  server_command(ed, serialcmd);
		  sprintf(kcamconf[chn].readmode, "rolling_sng");
		  
		  if (acq_was_on == 1) {
			system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
			acq_was_on = 0;
		  }
		  sprintf(loginfo, "%s", serialcmd);
		  log_action(loginfo);
		}
		
		// ---------------
		// ---------------
		
		else if (strncmp(str1, "rollingresetcds", strlen("rollingresetcds")) == 0) {
		  sprintf(serialcmd, "set mode %s", "rollingresetcds");
		  if (acq_is_on == 1) {
			go_fetcher("C-c");
			//system("tmux send-keys -t kcam_fetcher C-c");
			acq_was_on = 1;
		  }
		  server_command(ed, serialcmd);
		  sprintf(kcamconf[chn].readmode, "rolling_cds");
		  
		  if (acq_was_on == 1) {
			system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
			acq_was_on = 0;
		  }
		  sprintf(loginfo, "%s", serialcmd);
		  log_action(loginfo);
		}
		
		// ---------------
		// ---------------
		
		else if (strncmp(str1, "rollingresetnro", strlen("rollingresetnro")) == 0) {
		  sprintf(serialcmd, "set mode %s", "rollingresetnro");
		  if (acq_is_on == 1) {
			go_fetcher("C-c");
			//system("tmux send-keys -t kcam_fetcher C-c");
			acq_was_on = 1;
		  }
		  server_command(ed, serialcmd);
		  sprintf(kcamconf[chn].readmode, "rolling_nro");
		  
		  sprintf(serialcmd, "set imagetags on");
		  server_command(ed, serialcmd);
		  tagging = 1;
		  printf("Image tagging was turned on\n");
		  
		  if (acq_was_on == 1) {
			system("tmux send-keys -t kcam_fetcher \"./kcamfetch -u 1 -l 0\" C-m");
			acq_was_on = 0;
		  }
		  sprintf(loginfo, "%s", serialcmd);
		  log_action(loginfo);
		}
		
		// ---------------
		// ---------------
		
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
		printf("gain: \033[01;31m%f\033[00m\n", kcamconf[chn].gain);
		cmdOK = 1;
      }
	
    if (cmdOK == 0) // ------- set gain --------
      if (strncmp(cmdstring, "sgain", strlen("sgain")) == 0) {
		sscanf(cmdstring, "%s %f", str0, &fval);
		sprintf(serialcmd, "set gain %f", fval);
		server_command(ed, serialcmd);
		kcamconf[chn].gain = fval;
		sprintf(loginfo, "%s", serialcmd);
		log_action(loginfo);
		cmdOK = 1;
      }
	
    // ------------------------------------------------------------------------
    //                            EXPOSURE TIME
    // ------------------------------------------------------------------------
	
    if (cmdOK == 0) // ------- get exposure time --------
      if (strncmp(cmdstring, "gtint", strlen("gtint")) == 0) {
		sprintf(serialcmd, "fps raw");
		kcamconf[0].fps = server_query_float(ed, serialcmd);
		fval = 1000.0/kcamconf[0].fps; // from FPS to exposure time
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
		sprintf(loginfo, "%s", serialcmd);
		log_action(loginfo);
		cmdOK = 1;
	  }
    
    // ------------------------------------------------------------------------
    //                            COOLING INFO
    // ------------------------------------------------------------------------
    if (cmdOK == 0) // ------- get cryo temperature --------
      if (strncmp(cmdstring, "gtemp", strlen("gtemp")) == 0) {
		sprintf(serialcmd, "temperature cryostat ptcontroller raw");
		kcamconf[0].temperature = server_query_float(ed, serialcmd);
		printf("cryo temp: \033[01;31m%f\033[00m\n", kcamconf[0].temperature);
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
		sprintf(loginfo, "%s", serialcmd);
		log_action(loginfo);
		cmdOK = 1;
      }
	
    // ------------------------------------------------------------------------
    //                         CONVENIENCE TOOLS
    // ------------------------------------------------------------------------
    if (cmdOK == 0) // -------- get NDR ---------
      if (strncmp(cmdstring, "gNDR", strlen("gNDR")) == 0) {
		sprintf(serialcmd, "nbreadworeset raw");
		kcamconf[0].NDR = server_query_float(ed, serialcmd);
		printf("fps: \033[01;31m%d\033[00m\n", kcamconf[0].NDR);
		cmdOK = 1;
      }
	
	
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

