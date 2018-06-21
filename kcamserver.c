#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
  printf(fmt, "exit", "", "exit server");
  printf("%s", line);
  printf("%s", "\033[00m");
}

/* =========================================================================
 *                            Main program
 * ========================================================================= */
int main() {
  char prompt[200];
  char cmdstring[200];
  int cmdOK = 0;

  // --------------- initialize data structures -----------------
  
  // -------------- open a handle to the device -----------------
  
  // ---------------- command line interpreter ------------------
  sprintf(prompt, "\033[01;33m%s>\033[00m", "Kcam");
  for (;;) {
	cmdOK = 0;
	printf("%s ", prompt);
	fgets(cmdstring, 200, stdin);

	if (cmdOK == 0)
	  if (strncmp(cmdstring, "help", strlen("help")) == 0) {
		print_help();
		cmdOK = 1;
	  }

	if (cmdOK == 0)
	  if (strncmp(cmdstring, "quit", strlen("quit")) == 0) {
		printf("Bye!\n");
		exit(0);
	  }

	if (cmdOK == 0)
	  if (strncmp(cmdstring, "exit", strlen("quit")) == 0) {
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

