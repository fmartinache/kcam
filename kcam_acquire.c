/**
 * @file
 * An example program to show usage of EDT PCI DV library to acquire and
 * optionally save single or multiple images from devices connected to EDT
 * high speed digital imaging interface such as the PCI DV C-Link or PCI DV
 * FOX / RCX.
 * 
 * Provided as a starting point example for adding digital image acquisition
 * to a user application.  Includes optimization strategies that take
 * advantage of the EDT ring buffer library subroutines for pipelining image
 * acquisition and subsequent processing. This allows you to achieve higher
 * performance than would normally be possible through a basic acquire/process
 * scheme.
 *
 * The name is somewhat misleading -- because of the parallel aspect,
 * it really isn't the simplest way to do image acquisition.  For a
 * stone-simple example, see simplest_take.c.
 * 
 * For more more complex operations, including error detection, diagnostics,
 * changing camera exposure times, and tuning the acquisition in various
 * ways, refer to the take.c utility. For serial, see serial_cmd.c.
 * 
 * For a sample Windows GUI application code, see wintake.
 *     kcamconf[0].frameindex = i;
 * (C) 1997-2007 Engineering Design Team, Inc.
 */

/*
 * 
 * 
 * Compile with:
 * gcc kcam_acquire.c -o kcam_acquire -I/opt/EDTpdv -I/home/scexao/src/cacao/src/ImageStreamIO -I/home/scexao/src/cacao/src /home/scexao/src/cacao/src/ImageStreamIO/ImageStreamIO.c /opt/EDTpdv/libpdv.a -lm -lpthread -ldl 
 * 
 * 
 */

#include "edtinc.h"
#include "ImageStruct.h"
#include "ImageStreamIO.h"
#include "cred1struct.h"

CRED1STRUCT *kcamconf;

static void usage(char *progname, char *errmsg);
static void save_image(u_char * image_p, int width, int height, int depth,
		       char *basename, int count);

// ============================================================================
// ============================================================================
int main(int argc, char **argv) {
  int     i;
  int     unit = 1;
  int     overrun, overruns=0;
  int     timeout;
  int     timeouts, last_timeouts = 0;
  int     recovering_timeout = FALSE;
  char   *progname ;
  char   *cameratype;
  char    bmpfname[128];
  int     numbufs = 4;
  int     started;
  u_char *image_p;
  PdvDev *pdv_p;
  char    errstr[64];
  int     loops = 1;
  int     width, height, depth;
  char    edt_devname[128];
  int     channel = 0;
  
  unsigned short int *imageushort;
  double value_ave;
  int pix;

  int xsize, ysize;

  int kw;

  progname = argv[0];

  edt_devname[0] = '\0';
  *bmpfname = '\1';

  int SAVEBMP = 0;

  // --- process command line arguments ---

  --argc;
  ++argv;
  while (argc && ((argv[0][0] == '-') || (argv[0][0] == '/'))) {
    switch (argv[0][1]) {
      
    case 'N': // ------------------------------------------------------
      ++argv;
      --argc;
      if (argc < 1) {
	usage(progname, "Error: option 'N' requires a numeric argument\n");
      }
      if ((argv[0][0] >= '0') && (argv[0][0] <= '9')) {
	numbufs = atoi(argv[0]);
      }
      else {
	usage(progname, "Error: option 'N' requires a numeric argument\n");
      }
      break;
      
    case 'b': // ------------------------------------------------------
      ++argv;
      --argc;
      strcpy(bmpfname, argv[0]);
      SAVEBMP = 1;
      break;
      
    case 'l': // ------------------------------------------------------
      ++argv;
      --argc;
      if (argc < 1) {
	usage(progname, "Error: option 'l' requires a numeric argument\n");
      }
      if ((argv[0][0] >= '0') && (argv[0][0] <= '9')) {
	loops = atoi(argv[0]);
      }
      else {
	usage(progname, "Error: option 'l' requires a numeric argument\n");
      }
      break;
      
    case '-': // ------------------------------------------------------
      if (strcmp(argv[0], "--help") == 0) {
	usage(progname, "");
	exit(0);
      } else {
	fprintf(stderr, "unknown option: %s\n", argv[0]);
	usage(progname, "");
	exit(1);
      }
      break;

      
    default:
      fprintf(stderr, "unknown flag -'%c'\n", argv[0][1]);
    case '?':
    case 'h':
      usage(progname, "");
      exit(0);
    }
    argc--;
    argv++;
  }
  
  initCRED1STRUCT();
  printCRED1STRUCT(0);
  
  /*
   * open the interface
   * 
   * EDT_INTERFACE is defined in edtdef.h (included via edtinc.h)
   *
   */
  

  if ((pdv_p = pdv_open_channel(EDT_INTERFACE, unit, channel)) == NULL) {
    sprintf(errstr, "pdv_open_channel(%s%d_%d)", edt_devname, unit, channel);
    pdv_perror(errstr);
    return (1);
  }
  
  pdv_flush_fifo(pdv_p);
  
  
  IMAGE *imarray;    // pointer to array of images
  int NBIMAGES = 1;  // can hold 1 image
  long naxis;        // number of axis
  uint8_t atype;     // data type
  uint32_t *imsize;  // image size 
  int shared;        // 1 if image in shared memory
  int NBkw;          // number of keywords supported
  
  xsize =  kcamconf[0].row1 - kcamconf[0].row0 + 1;
  ysize = (kcamconf[0].col1 - kcamconf[0].col0) * 32 + 1;
  printf("row0 & row1  : %d & %d\n",kcamconf[0].row0 , kcamconf[0].row1);
  printf("col0 & col1  : %d & %d\n",kcamconf[0].col0 , kcamconf[0].col1);

  //pdv_set_width(pdv_p, ysize);
  //pdv_set_height(pdv_p, xsize);
    
  width      = pdv_get_width(pdv_p);
  height     = pdv_get_height(pdv_p);
  depth      = pdv_get_depth(pdv_p);
  timeout    = pdv_get_timeout(pdv_p);
  cameratype = pdv_get_cameratype(pdv_p);
  
  printf("image size  : %d x %d\n", width, height);
  printf("Timeout     : %d\n", timeout);
  printf("Camera type : %s\n", cameratype);
  
  // allocate memory for array of images
  imarray = (IMAGE*) malloc(sizeof(IMAGE)*NBIMAGES);
  naxis = 2;
  imsize = (uint32_t *) malloc(sizeof(uint32_t)*naxis);
  imsize[0] = width;
  imsize[1] = height;	
  atype = _DATATYPE_UINT16;
  // image will be in shared memory
  shared = 1;
  // allocate space for 10 keywords
  NBkw = 10;
  ImageStreamIO_createIm(&imarray[0], "kcam", naxis, imsize, atype,
			 shared, NBkw);
  free(imsize);
    
  // Add keywords
  kw = 0;
  strcpy(imarray[0].kw[kw].name, "tint");
  imarray[0].kw[kw].type = 'D';
  imarray[0].kw[kw].value.numf = kcamconf[0].tint;
  strcpy(imarray[0].kw[kw].comment, "exposure time");

  kw = 1;
  strcpy(imarray[0].kw[kw].name, "fps");
  imarray[0].kw[kw].type = 'D';
  imarray[0].kw[kw].value.numf = kcamconf[0].fps;
  strcpy(imarray[0].kw[kw].comment, "frame rate");
  
  kw = 2;
  strcpy(imarray[0].kw[kw].name, "NDR");
  imarray[0].kw[kw].type = 'L';
  imarray[0].kw[kw].value.numl = kcamconf[0].NDR;
  strcpy(imarray[0].kw[kw].comment, "NDR");
  
  kw = 3;
  strcpy(imarray[0].kw[kw].name, "row0");
  imarray[0].kw[kw].type = 'L';
  imarray[0].kw[kw].value.numl = kcamconf[0].row0;
  strcpy(imarray[0].kw[kw].comment, "row0 (range 1-256)");
  
  kw = 4;
  strcpy(imarray[0].kw[kw].name, "row1");
  imarray[0].kw[kw].type = 'L';
  imarray[0].kw[kw].value.numl = kcamconf[0].row1;
  strcpy(imarray[0].kw[kw].comment, "row1 (range 1-256)");
  
  kw = 5;
  strcpy(imarray[0].kw[kw].name, "col0");
  imarray[0].kw[kw].type = 'L';
  imarray[0].kw[kw].value.numl = kcamconf[0].col0;
  strcpy(imarray[0].kw[kw].comment, "col0 (range 1-10)");
  
  kw = 6;
  strcpy(imarray[0].kw[kw].name, "col1");
  imarray[0].kw[kw].type = 'L';
  imarray[0].kw[kw].value.numl = kcamconf[0].col1;
  strcpy(imarray[0].kw[kw].comment, "col1 (range 1-10)");

  kw = 7;
  strcpy(imarray[0].kw[kw].name, "temp");
  imarray[0].kw[kw].type = 'D';
  imarray[0].kw[kw].value.numf = kcamconf[0].temperature;
  strcpy(imarray[0].kw[kw].comment, "detector temperature");
  
  kw = 8;
  strcpy(imarray[0].kw[kw].name, "mode");
  imarray[0].kw[kw].type = 'S';
  strcpy(imarray[0].kw[kw].value.valstr, kcamconf[0].mode);
  //imarray[0].kw[kw].value.numf = kcamconf[0].mo;
  strcpy(imarray[0].kw[kw].comment, "readout mode");

  // other keywords to add?
  // - timestamp
  // - gain

  fflush(stdout);
  
  /*
   * allocate four buffers for optimal pdv ring buffer pipeline (reduce if
   * memory is at a premium)
   */
  pdv_multibuf(pdv_p, numbufs);
  
  printf("reading %d image%s from '%s'\nwidth %d height %d depth %d\n",
		 loops, loops == 1 ? "" : "s", cameratype, width, height, depth);
  
  // imageushort = (unsigned short *) malloc(sizeof(unsigned short)*width*height);

  /*
   * prestart the first image or images outside the loop to get the
   * pipeline going. Start multiple images unless force_single set in
   * config file, since some cameras (e.g. ones that need a gap between
   * images or that take a serial command to start every image) don't
   * tolerate queueing of multiple images
   */

  if (pdv_p->dd_p->force_single) {
    pdv_start_image(pdv_p);
    started = 1;
  }
  else {
    pdv_start_images(pdv_p, numbufs);
    started = numbufs;
  }
  printf("\n");
  i = 0;

  int loopOK = 1;

  while(loopOK == 1) {

    // update shared memory keywords that can be updated without restart
    imarray[0].kw[0].value.numf = kcamconf[0].tint;
    imarray[0].kw[1].value.numf = kcamconf[0].fps;
    imarray[0].kw[2].value.numl = kcamconf[0].NDR;
    imarray[0].kw[7].value.numf = kcamconf[0].temperature;

    /*
     * get the image and immediately start the next one (if not the last
     * time through the loop). Processing (saving to a file in this case)
     * can then occur in parallel with the next acquisition
     */
    
    image_p = pdv_wait_image(pdv_p);
    
    if ((overrun = (edt_reg_read(pdv_p, PDV_STAT) & PDV_OVERRUN)))
      ++overruns;
    
    pdv_start_image(pdv_p);
    timeouts = pdv_timeouts(pdv_p);
    
    /*
     * check for timeouts or data overruns -- timeouts occur when data
     * is lost, camera isn't hooked up, etc, and application programs
     * should always check for them. data overruns usually occur as a
     * result of a timeout but should be checked for separately since
     * ROI can sometimes mask timeouts
     */
    if (timeouts > last_timeouts) {
      /*
       * pdv_timeout_cleanup helps recover gracefully after a timeout,
       * particularly if multiple buffers were prestarted
       */
      pdv_timeout_restart(pdv_p, TRUE);
      last_timeouts = timeouts;
      recovering_timeout = TRUE;
      printf("\ntimeout....\n");
    }
    else if (recovering_timeout) {
      pdv_timeout_restart(pdv_p, TRUE);
      recovering_timeout = FALSE;
      printf("\nrestarted....\n");
    }
    
    // printf("line = %d\n", __LINE__);
    fflush(stdout);
    
    if(SAVEBMP == 1) {
      if (*bmpfname)
	save_image(image_p, width, height, depth, bmpfname, (loops > 1?i:-1));
    }
    
    imageushort = (unsigned short *) image_p;
    
    fflush(stdout);
    
    imarray[0].md[0].write = 1; // set this flag to 1 when writing data
    
    fflush(stdout);
    
    printf("w h = %d %d\n", width, height);
    
    memcpy(imarray[0].array.UI16, imageushort,
	   sizeof(unsigned short)*width*height);
    
    
    fflush(stdout);
    
    imarray[0].md[0].write = 0;
    // POST ALL SEMAPHORES
    ImageStreamIO_sempost(&imarray[0], -1);
    
    printf("line = %d\n", __LINE__);
    fflush(stdout);
    
    imarray[0].md[0].write = 0; // Done writing data
    imarray[0].md[0].cnt0++;
    imarray[0].md[0].cnt1++;
    
    fflush(stdout);
        
    value_ave = 0.0;
    for (pix=0; pix < width*height; pix++)
      value_ave += imageushort[pix];
    value_ave /= width*height;
    kcamconf[0].frameindex = i;
    printf("\r image %10d   Average value = %20lf         ", i, value_ave);
    
    i++;
    if (i==loops)
      loopOK = 0;
    
    fflush(stdout);
    
  }
  puts("");
  
  printf("%d images %d timeouts %d overruns\n", loops, last_timeouts, overruns);
  
  /*
   * if we got timeouts it indicates there is a problem
   */
  if (last_timeouts) printf("check camera and connections\n");
  pdv_close(pdv_p);
  
  if (overruns || timeouts) exit(2);
  
  //free(imageushort);
  free(imarray);
  
  exit(0);
}



// ============================================================================
// ============================================================================
static void save_image(u_char * image_p, int s_width, int s_height,
					   int s_depth, char *tmpname, int count) {
  int     s_db = bits2bytes(s_depth);
  char    fname[256];
  
  u_char *bbuf = NULL;
  if ((strcmp(&tmpname[strlen(tmpname) - 4], ".bmp") == 0)
	  || (strcmp(&tmpname[strlen(tmpname) - 4], ".BMP") == 0))
	tmpname[strlen(tmpname) - 4] = '\0';

  if (count >= 0) sprintf(fname, "%s_%03d.bmp", tmpname, count);
  else            sprintf(fname, "%s.bmp", tmpname);
  
  switch (s_db) {
  case 1: // ------------------------------------------------------
	dvu_write_bmp(fname, image_p, s_width, s_height);
	printf("writing %dx%dx%d bitmap file to %s\n",
		   s_width, s_height, s_depth, fname);
	break;

  case 2: // ------------------------------------------------------
	printf("converting %dx%dx%d image to 8 bits, writing to %s\n",
		   s_width, s_height, s_depth, fname);

	if (!bbuf)
	  bbuf = (u_char *) pdv_alloc(s_width * s_height);

	if (bbuf == NULL) {
	  pdv_perror("data buf malloc");
	  exit(1);
	}
	dvu_word2byte((u_short *) image_p, (u_char *) bbuf,
				  s_width * s_height, s_depth);
	dvu_write_bmp(fname, bbuf, s_width, s_height);
	break;

  case 3: // ------------------------------------------------------
	printf("writing %dx%dx%d bmp file to %s\n",
		   s_width, s_height, s_depth, fname);
	
	dvu_write_bmp_24(fname, (u_char *) image_p, s_width, s_height);
	break;
	
  default: // ------------------------------------------------------
	printf("invalid image depth for file write...!\n");
	break;
  }
}

// ============================================================================
// ============================================================================
static void usage(char *progname, char *errmsg) {
  puts(errmsg);
  printf("%s: simple example program that acquires images from an\n", progname);
  printf("EDT digital imaging interface board (PCI DV, PCI DVK, etc.)\n");
  puts("");
  printf("usage: %s [-b fname] [-l loops] [-N numbufs] [-u unit] [-c channel]\n",
		 progname);
  printf("  -b fname     output to MS bitmap file\n");
  printf("  -l loops     number of loops (images to take)\n");
  printf("  -N numbufs   number of ring buffers (see users guide) (default 4)\n");
  printf("  -h           this help message\n");
  exit(1);
}
