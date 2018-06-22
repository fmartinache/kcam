#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "cred1struct.h"



extern CRED1STRUCT *kcamconf;

// ============================================================================
// ============================================================================
int printCRED1STRUCT(int cam) {
  char ifmt[20] = "%25s %11d %3s\n";
  char ffmt[20] = "%25s %11.5f %3s\n";
  char cfmt[20] = "%25s %15s\n";
  char line[80] = 
    "============================================================\n";
  printf("%s", "\033[01;36m");
  printf("%s", line);
  printf(ifmt, "unit", cam, "");
  printf("%s", line);
  printf(ifmt, "FGchannel",  kcamconf[cam].FGchannel, "");
  printf(ffmt, "Exp. time",  kcamconf[cam].tint, "us");
  printf(ffmt, "Frame rate", kcamconf[cam].fps, "Hz");
  printf(ffmt, "Max FPS",    kcamconf[cam].maxFps, "Hz");
  printf(ifmt,  "NDR",       kcamconf[cam].NDR, "");
  printf(ffmt, "Cryo temp.", kcamconf[cam].temperature, "K");
  printf(ifmt, "Current frame", kcamconf[cam].frameindex, "");
  printf("%s", line);

  //kcamconf[cam].temperature_setpoint);
  
  printf(ifmt, "cropping ON=", kcamconf[cam].cropmode, "");
  printf("%25s %3d-%3d\n", "cropping row", kcamconf[cam].row0, kcamconf[cam].row1);
  printf("%25s %3d-%3d\n", "cropping col", kcamconf[cam].col0, kcamconf[cam].col1);
  printf("%25s %3d x %3d\n", 
	 "Resulting window",
	 kcamconf[cam].row1 - kcamconf[cam].row0,
	 (kcamconf[cam].col1 - kcamconf[cam].col0) * 32);
  printf("%s", line);
  printf(cfmt, "readout mode", kcamconf[cam].readmode);
  printf("%s", line);
  printf("%s", "\033[00m");
  return (0);
}

// ============================================================================
// ============================================================================
int initCRED1STRUCT() {
  int SM_fd;        // shared memory file descriptor
  int create = 0;   // 1 if we need to re-create shared memory
  struct stat file_stat;
	
  SM_fd = open(kcamconf_name, O_RDWR);
  if(SM_fd==-1) {
    printf("Cannot import file \"%s\" -> creating file\n", kcamconf_name);
    create = 1;
  }
  else {
    fstat(SM_fd, &file_stat);  // read file stats
    printf("File %s size: %zd\n", kcamconf_name, file_stat.st_size);
    if(file_stat.st_size!=sizeof(CRED1STRUCT)*NBconf) {
      printf("File \"%s\" size is wrong -> recreating file\n", kcamconf_name);
      create = 1;
      close(SM_fd);
    }
  }

  if(create==1) {
    printf("======== CREATING SHARED MEMORY FILE =======\n");
    int result;

    SM_fd = open(kcamconf_name, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);

    if (SM_fd == -1) {
      perror("Error opening file for writing");
      exit(0);
    }

    result = lseek(SM_fd, sizeof(CRED1STRUCT)*NBconf-1, SEEK_SET);
    if (result == -1) {
      close(SM_fd);
      perror("Error calling lseek() to 'stretch' the file");
      exit(0);
    }
    
    result = write(SM_fd, "", 1);
    if (result != 1) {
      close(SM_fd);
      perror("Error writing last byte of the file");
      exit(0);
    }
  }
  

  kcamconf = (CRED1STRUCT*) mmap(0, sizeof(CRED1STRUCT)*NBconf, 
				  PROT_READ | PROT_WRITE, 
				 MAP_SHARED, SM_fd, 0);
  if (kcamconf == MAP_FAILED) {
    close(SM_fd);
    perror("Error mmapping the file");
    exit(0);
  }
	
  return 0;
}

