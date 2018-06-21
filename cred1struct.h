#ifndef _CRED1STRUCT_H
#define _CRED1STRUCT_H
#define kcamconf_name "/tmp/kcamconf.shm"

#define NBconf  2

typedef struct {		
  int FGchannel;          // frame grabber channel 
  float tint;             // integration time for each read	
  int   NDR;              // number of reads per reset	
  float fps;              // current number of frames in Hz
  char mode[16];          // readout mode
  float temperature;
	  
  // cropping parameters
  int cropmode;          // 0: OFF, 1: ON
  int row0; // range 1 - 256 (granularity = 1)
  int row1; // range 1 - 256 (granularity = 1)
  int col0; // range 1 -  10 (granularity = 32)
  int col1; // range 1 -  10 (granularity = 32)
  
  int sensibility;
  // 0: low
  // 1: medium
  // 2: high
  
  long frameindex;
  
} CRED1STRUCT;


int printCRED1STRUCT(int cam);
int initCRED1STRUCT();

#endif
