#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "cred1struct.h"



extern CRED1STRUCT *kcamconf;

int printCRED1STRUCT(int cam) {
  printf("cam = %d\n", cam);
  printf("======================================================================\n");
  printf("FGchannel    %d\n", kcamconf[cam].FGchannel);
  printf("tint         %f\n", kcamconf[cam].tint);
  printf("NDR val      %d\n", kcamconf[cam].NDR);
  
  
  printf("Frame rate             %12f Hz\n", kcamconf[cam].fps);	
  
  printf("Detector temperature   %12f\n", 
	 kcamconf[cam].temperature);
    //kcamconf[cam].temperature_setpoint);
  
  printf("cropping   ON=%d   window  x:%3d-%3d  y:%3d-%3d\n", kcamconf[cam].cropmode,
	 kcamconf[cam].row0, kcamconf[cam].row1, 
	 kcamconf[cam].col0, kcamconf[cam].col1);
	
  printf("Current frame = %ld\n", kcamconf[cam].frameindex);
  printf("======================================================================\n");
  
  return (0);
}


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
				  PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);
  if (kcamconf == MAP_FAILED) {
    close(SM_fd);
    perror("Error mmapping the file");
    exit(0);
  }
	
  return 0;
}

