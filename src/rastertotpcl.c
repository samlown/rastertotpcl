/*
 *   Toshiba TEC TPCL Label printer filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2001-2007 by Easy Software Products.
 *   Copyright 2009 by Patrick Kong
 *   Copyright 2010 by Sam Lown
 *
 *   Based on Source from CUPS printing system and rastertolabel filter.
 * 
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *
 *  Structure based on CUPS rastertolabel by Easy Software Products, 2007.
 *  Base version by Patrick Kong, 2009-07-21
 *  TOPIX Compression added by Sam Lown, 2010-05-24
 *
 * Contents:
 *
 *   Setup()        - Prepare the printer for printing.
 *   StartPage()    - Start a page of graphics.
 *   EndPage()      - Finish a page of graphics.
 *   CancelJob()    - Cancel the current job...
 *   OutputLine()   - Output a line of graphics.
 *   main()         - Main entry and processing of driver.
 *
 *   TOPIXCompress() - Compress output into TEC's TOPIX format.
 *   TOPIXCompressOutputBuffer() - Send current contents of TOPIX data to stdout.
 *
 * This driver should support all Toshiba TEC Label Printers with support for TPCL (TEC
 * Printer Command Language) and TOPIX Compression for graphics. 
 *
 */

#include <cups/cups.h>
#include <cups/raster.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>


/*
 * Model number constants...
 */
#define INTSIZE		20 			/* MAXIMUM CHARACTERS INTEGER */

/*
 * TEC Graphics Modes
 */
#define TEC_GMODE_TOPIX   3
#define TEC_GMODE_HEX_AND 1
#define TEC_GMODE_HEX_OR  5


/*
 * Globals...
 */
static unsigned char	*Buffer;		     /* Output buffer */
static unsigned char	*LastBuffer;		 /* Last buffer */
static unsigned char  *CompBuffer;     /* Byte array of whole image */
unsigned char         *CompBufferPtr;  /* Pointer to current position in CompBuffer */
int   CompLastLine;   /* Last line number sent to TOPIX output */
int   Page,           /* Current page */
      Feed,           /* Number of lines to skip */
      Canceled,		    /* Non-zero if job is canceled */
      Gmode; 			    /* Tec Graphics mode */

int		ModelNumber; 		/* cupsModelNumber attribute (not currently in use) */

/*
 * Prototypes...
 */
void Setup(ppd_file_t *ppd);
void StartPage(ppd_file_t *ppd, cups_page_header2_t *header);
void EndPage(ppd_file_t *ppd, cups_page_header2_t *header);
void CancelJob(int sig);
void OutputLine(ppd_file_t *ppd, cups_page_header2_t *header, int y);

void TOPIXCompress(ppd_file_t *ppd, cups_page_header2_t *header, int y);
void TOPIXCompressOutputBuffer(ppd_file_t *ppd, cups_page_header2_t *header, int y);

/*
 * 'Setup()' - Prepare the printer for printing.
 */
void Setup(ppd_file_t *ppd)			/* I - PPD file */
{
  char		*Fadjm;			/* Fine adjust printing position */
  char		*Radj;			/* Ribbon adjust parameter */
  ppd_choice_t	*choice;		/* Marked choice */
  /* initialize Fadjm */
  Fadjm = (char *) malloc(INTSIZE +2); /* Advanced parameters for printer */
  Radj  = (char *) malloc(INTSIZE +2); /* Ribbon ajust parameter */

  /*
   * Get the model number from the PPD file.
   * This is not yet used for anything.
   */
  ModelNumber = ppd->model_number;
  
  /*
   * Always send a reset command. Helps with reliability on failed jobs.
   */
  puts("{WS|}");

  /*  
   * Modification to take in consideration feed ajust reverse feed etc
   */
  strcpy(Fadjm,"{AX;"); /* Place command start */
  /* feed adjust */
  choice = ppdFindMarkedChoice(ppd, "FAdjSgn");
  switch (atoi(choice->choice))
  {
    case 0 :
      strcat(Fadjm,"+");
      break;
    case 1 :
      strcat(Fadjm,"-");
      break;
    default :
      strcat(Fadjm,"+");
      break;
  }
  choice = ppdFindMarkedChoice(ppd, "FAdjV");
  strcat(Fadjm,choice->choice);
  
  /* Cut adjust peel adjust */
  choice = ppdFindMarkedChoice(ppd, "CAdjSgn");
  switch (atoi(choice->choice))
  {
    case 0 :
      strcat(Fadjm,",+");
      break;
    case 1 :
      strcat(Fadjm,",-");
      break;
    default :
      strcat(Fadjm,",+");
      break;
  }
  choice = ppdFindMarkedChoice(ppd, "CAdjV");
  strcat(Fadjm,choice->choice);

  /* back feed adjust */
  choice = ppdFindMarkedChoice(ppd, "RAdjSgn");
  switch (atoi(choice->choice))
  {
    case 0 :
      strcat(Fadjm,",+");
      break;
    case 1 :
      strcat(Fadjm,",-");
      break;
    default :
      strcat(Fadjm,",+");
      break;
  }

  choice = ppdFindMarkedChoice(ppd, "RAdjV");
  strcat(Fadjm,choice->choice);
  
  /* close the command */
  strcat(Fadjm,"|}");
  /* send the command */
  puts(Fadjm); /*send advanced parameters */

  /* Send ribbon Motor setup parameters */
  strcpy(Radj,"{RM;");	/* start command for ribbon */
  choice = ppdFindMarkedChoice(ppd, "RbnAdjFwd");
  strcat(Radj,choice->choice); /* value for take up motor */
  choice = ppdFindMarkedChoice(ppd, "RbnAdjBck");
  strcat(Radj,choice->choice);
  strcat(Radj,"|}");
  puts(Radj);

}


/*
 * 'StartPage()' - Start a page of graphics.
 */
void
StartPage(ppd_file_t         *ppd,	/* I - PPD file */
          cups_page_header2_t *header)	/* I - Page header */
{
  ppd_choice_t  *choice;		/* Marked choice */
  int           labelgap;		/* length of labelgap */
  int           labelpitch; /* label pitch, distance from start of one label to the next */
  int         	length;			/* Effective label length */
  int 		      width;			/* Effective label width */
  char		      *Fadjt;			/* Fine adjust temperature */
  
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */

  Fadjt = (char *) malloc(INTSIZE +2);

  /*
   * Show page device dictionary...
   */
  fprintf(stderr, "DEBUG: StartPage...\n");
  fprintf(stderr, "DEBUG: MediaClass = \"%s\"\n", header->MediaClass);
  fprintf(stderr, "DEBUG: MediaColor = \"%s\"\n", header->MediaColor);
  fprintf(stderr, "DEBUG: MediaType = \"%s\"\n", header->MediaType);
  fprintf(stderr, "DEBUG: OutputType = \"%s\"\n", header->OutputType);

  fprintf(stderr, "DEBUG: AdvanceDistance = %d\n", header->AdvanceDistance);
  fprintf(stderr, "DEBUG: AdvanceMedia = %d\n", header->AdvanceMedia);
  fprintf(stderr, "DEBUG: Collate = %d\n", header->Collate);
  fprintf(stderr, "DEBUG: CutMedia = %d\n", header->CutMedia);
  fprintf(stderr, "DEBUG: Duplex = %d\n", header->Duplex);
  fprintf(stderr, "DEBUG: HWResolution = [ %d %d ]\n", header->HWResolution[0],
          header->HWResolution[1]);
  fprintf(stderr, "DEBUG: ImagingBoundingBox = [ %d %d %d %d ]\n",
          header->ImagingBoundingBox[0], header->ImagingBoundingBox[1],
          header->ImagingBoundingBox[2], header->ImagingBoundingBox[3]);
  fprintf(stderr, "DEBUG: InsertSheet = %d\n", header->InsertSheet);
  fprintf(stderr, "DEBUG: Jog = %d\n", header->Jog);
  fprintf(stderr, "DEBUG: LeadingEdge = %d\n", header->LeadingEdge);
  fprintf(stderr, "DEBUG: Margins = [ %d %d ]\n", header->Margins[0],
          header->Margins[1]);
  fprintf(stderr, "DEBUG: ManualFeed = %d\n", header->ManualFeed);
  fprintf(stderr, "DEBUG: MediaPosition = %d\n", header->MediaPosition);
  fprintf(stderr, "DEBUG: MediaWeight = %d\n", header->MediaWeight);
  fprintf(stderr, "DEBUG: MirrorPrint = %d\n", header->MirrorPrint);
  fprintf(stderr, "DEBUG: NegativePrint = %d\n", header->NegativePrint);
  fprintf(stderr, "DEBUG: NumCopies = %d\n", header->NumCopies);
  fprintf(stderr, "DEBUG: Orientation = %d\n", header->Orientation);
  fprintf(stderr, "DEBUG: OutputFaceUp = %d\n", header->OutputFaceUp);
  fprintf(stderr, "DEBUG: cupsPageSize = [ %f %f ]\n", header->cupsPageSize[0],
          header->cupsPageSize[1]);
  fprintf(stderr, "DEBUG: Separations = %d\n", header->Separations);
  fprintf(stderr, "DEBUG: TraySwitch = %d\n", header->TraySwitch);
  fprintf(stderr, "DEBUG: Tumble = %d\n", header->Tumble);
  fprintf(stderr, "DEBUG: cupsWidth = %d\n", header->cupsWidth);
  fprintf(stderr, "DEBUG: cupsHeight = %d\n", header->cupsHeight);
  fprintf(stderr, "DEBUG: cupsMediaType = %d\n", header->cupsMediaType);
  fprintf(stderr, "DEBUG: cupsBitsPerColor = %d\n", header->cupsBitsPerColor);
  fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d\n", header->cupsBitsPerPixel);
  fprintf(stderr, "DEBUG: cupsBytesPerLine = %d\n", header->cupsBytesPerLine);
  fprintf(stderr, "DEBUG: cupsColorOrder = %d\n", header->cupsColorOrder);
  fprintf(stderr, "DEBUG: cupsColorSpace = %d\n", header->cupsColorSpace);
  fprintf(stderr, "DEBUG: cupsCompression = %d\n", header->cupsCompression);

  /*
   * Register a signal handler to eject the current page if the
   * job is canceled.
   */
#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, CancelJob);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = CancelJob;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, CancelJob);
#endif /* HAVE_SIGSET */

  // printf("{XJ;Page Start|}");
  
  /*
   * First paper size Dxxxx,xxxx,xxxx
   * 
   *   100 == 10.0mm
   */

  /* Get labelgap for printing */
  choice = ppdFindMarkedChoice(ppd, "Gap");
  labelgap = atoi(choice->choice) * 10;

  /* Calculate page widths and heights */
  length = (int) (header->cupsPageSize[1] * 254/72);
  labelpitch = length + labelgap;
  width = (int) (header->cupsPageSize[0] * 254/72);

  /* Send label size, assume gap is same all the way round */
  // printf("{D%04d,%04d,%04d|}\n",labelpitch, width, length, width + labelgap); 
  printf("{D%04d,%04d,%04d,%04d|}\n",labelpitch, width, length, width + labelgap); 

  /*
   * Place the right command in the parameter AY temperature fine adjust
   * Uses number from PPD less 11.
   */
  switch (header->cupsCompression)
  {
    case 1 :
      strcpy(Fadjt,"{AY;-10,");
      break;
    case 2 :
      strcpy(Fadjt,"{AY;-09,");
      break;
    case 3 :
      strcpy(Fadjt,"{AY;-08,");
      break;
    case 4 :
      strcpy(Fadjt,"{AY;-07,");
      break;
    case 5 :
      strcpy(Fadjt,"{AY;-06,");
      break;
    case 6 :
      strcpy(Fadjt,"{AY;-05,");
      break;
    case 7 :
      strcpy(Fadjt,"{AY;-04,");
      break;
    case 8 :
      strcpy(Fadjt,"{AY;-03,");
      break;
    case 9 :
      strcpy(Fadjt,"{AY;-02,");
      break;
    case 10 :
      strcpy(Fadjt,"{AY;-01,");
      break;
    case 11 :
      strcpy(Fadjt,"{AY;+00,");
      break;
    case 12 :
      strcpy(Fadjt,"{AY;+01,");
      break;
    case 13 :
      strcpy(Fadjt,"{AY;+02,");
      break;
    case 14 :
      strcpy(Fadjt,"{AY;+03,");
      break;
    case 15 :
      strcpy(Fadjt,"{AY;+04,");
      break;
    case 16 :
      strcpy(Fadjt,"{AY;+05,");
      break;
    case 17 :
      strcpy(Fadjt,"{AY;+06,");
      break;
    case 18 :
      strcpy(Fadjt,"{AY;+07,");
      break;
    case 19 :
      strcpy(Fadjt,"{AY;+08,");
      break;
    case 20 :
      strcpy(Fadjt,"{AY;+09,");
      break;
    case 21 :
      strcpy(Fadjt,"{AY;+10,");
      break;
  }
  
  /*
   * Completing fine adjust according to Thermal or direct printing
   */
  if (strcmp(header->MediaType, "Direct") == 0)
    strcat(Fadjt,"1|}");
  else // Thermal transfer mode, with or without ribbon saving
    strcat(Fadjt,"0|}");
  
  /*
   * Send parameter to printer
   */
  puts(Fadjt);

  //printf("{T|}\n");   /* Feed one sheet of paper */
  printf("{C|}\n"); 	/* clear image buffer */

  /* Get graphics mode from ppd file for graphics drawing */
  choice = ppdFindMarkedChoice(ppd,"teGraphicsMode");
  switch (atoi(choice->choice)) {
    case 3:
      Gmode = TEC_GMODE_HEX_OR; // OR drawing hex mode
      break;
    case 2:
      Gmode = TEC_GMODE_HEX_AND; // AND drawing hex mode
      break;
    case 1:
    default:
      Gmode = TEC_GMODE_TOPIX;
  }

  // Only print the graphics if NOT in TOPIX mode!
  if (Gmode != TEC_GMODE_TOPIX)
  {
    printf("{SG;0000,0000,%04d,%04d,%d,", header->cupsBytesPerLine * 8, header->cupsHeight, Gmode);
  }
  else
  {
    /*
     * Allocate buffers for 8 dots per byte graphics ready for TOPIX compression
     */
    LastBuffer = malloc(header->cupsBytesPerLine);
    memset(LastBuffer, 0, header->cupsBytesPerLine);
    // Allocate big chunk of memory for parts of TOPIX image 
    CompBuffer = malloc(0xFFFF);
    memset(CompBuffer, 0, 0xFFFF);
    CompBufferPtr = CompBuffer;
    CompLastLine = 0;
  }

  /*
   * Allocate memory for a line of graphics...
   */
  Buffer = malloc(header->cupsBytesPerLine);
  Feed   = 0;
}


/*
 * 'EndPage()' - Finish a page of graphics.
 */
void
EndPage(ppd_file_t *ppd,		/* I - PPD file */
        cups_page_header2_t *header)	/* I - Page header */
{
  int 		      Quant;	 		/* Quantity to print */
  char		      *Temp;			/* Temporary string */
  unsigned int 	Tmedia;			/* type of media */
  char          *Tmode;			/* Print mode */
  unsigned int  Tmirror;		/* Mirror print */
  unsigned int  tstat;			/* with or without status */
  unsigned int  detect;			/* type of label sensor*/
  char  	      *Tspeed;		/* print Speed */
  unsigned int  Tcut;			  /* Cut quantity */
  unsigned int  CutActive;	/* Activate cutter */
  ppd_choice_t  *choice;		/* Marked choice */

#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */

  Temp = (char *) malloc(INTSIZE +2);
  Tmode = (char *) malloc(INTSIZE +2);
  Tspeed = (char *) malloc(INTSIZE +2);
  detect = 0;
  Quant = 1;
  CutActive =0;

  /* Initialise printing defaults */
	Tmedia =0;
	Tmirror=0;
	tstat =0;
	strcpy(Tmode,"C\0");
	strcpy(Tspeed,"3\0");

  /*
   * Terminate sending graphics.
   * If not in TOPIX mode, we also need to close the raw graphics output.
   */
  if (Gmode == TEC_GMODE_TOPIX)
    TOPIXCompressOutputBuffer(ppd, header, 0);
  else
    printf("|}\n");


  if (Canceled)
  {
    /*
     * Ramclear in case of error
     */
    puts("{WR|}");

  } else {

    /*
     * Set media tracking...
     */
    if (ppdIsMarked(ppd, "teMediaTracking", "0"))
      detect = 0;
    else if (ppdIsMarked(ppd, "teMediaTracking", "1"))
      detect = 1;
    else if (ppdIsMarked(ppd, "teMediaTracking", "2"))
      detect = 2;
    else if (ppdIsMarked(ppd, "teMediaTracking", "3"))
      detect= 3;
    else if (ppdIsMarked(ppd, "teMediaTracking", "4"))
      detect = 4;

    //	printf("{XJ;End Page %d|}",detect);
    /*
     * Set print mode...
     */
    if (header->CutMedia) /* coupe active */
    {	
      strcpy(Tmode,"C\0");
      CutActive =1;
    }
    else
    {
      if ((choice = ppdFindMarkedChoice(ppd, "tePrintMode")) != NULL &&
        strcmp(choice->choice, "0"))
      {
        strcpy(Tmode,"C\0");
        if (!strcmp(choice->choice,"1"))
          strcpy(Tmode,"D\0");
        else if (!strcmp(choice->choice, "2"))
          strcpy(Tmode,"E\0");
        else if (!strcmp(choice->choice, "3"))
        {
          strcpy(Tmode,"C\0");
          CutActive =1;
        }
      }
    }
    /*
     * Set print rate...
     */
    choice = ppdFindMarkedChoice(ppd, "tePrintRate");

    /* The speed is selected from the printer parameter choice */
    switch (atoi(choice->choice))
    {
      case 2 :
        strcpy(Tspeed,"2\0");
        break;
      case 3 :
        strcpy(Tspeed,"3\0");
        break;
      case 4 :
        strcpy(Tspeed,"4\0");
        break;
      case 5 :
        strcpy(Tspeed,"5\0");
        break;
      case 6 :
        strcpy(Tspeed,"6\0");
        break;
      case 8 :
        strcpy(Tspeed,"8\0");
        break;
      case 10 :
        strcpy(Tspeed,"A\0");
        break;
    }
    
    /*
     * Set with or without ribbon mode from media type 
     */
    if (!strcmp(header->MediaType, "Direct"))
      Tmedia = 0;
    else if (!strcmp(header->MediaType, "Thermal"))
      Tmedia = 1;
    else if (!strcmp(header->MediaType,"Thermal2"))
      Tmedia = 2;
   
    /* status response */
    tstat = 0;

    /*
     * Manage the cut option every label or end of batch print 
     */
    switch (header->cupsRowStep)
    {
      case 0 :
        Tcut =0;
        break;
      case 1 :
        Tcut =1;
        break;
      case 999 :
        Tcut =0;
        break;
      default:
        Tcut= 0;
        break;
    }

    /*
     * Version 1.2 Mirror option not managed local management
     */ 
    if ((choice = ppdFindMarkedChoice(ppd, "PrintOrient")) != NULL)
      Tmirror = atoi(choice->choice);
    else
      Tmirror = 0;

    /*
     * End the label and eject...
     */
    // printf("{PV00;0010,%4d,0020,0020,A,00,B=----Hello Linux World From S.K.E----- |}\n",header->PageSize[1]*254/72 - 50);
    // printf("{PC01;0010,%4d,05,05,O,00,B= Only Man gives names and value to things (P.Kong)|}\n",header->PageSize[1]*254/72 - 30);
    printf("{XS;I,%04d,%03d%d%s%s%d%d%d|}\n",header->NumCopies,Tcut,detect,Tmode,Tspeed,Tmedia,Tmirror,tstat);
    
    /* Send eject command if cut active */
    if (CutActive > 0)
      printf("{IB|}\n");

  } // Not Cancelled


  fflush(stdout);

  /*
   * Unregister the signal handler...
   */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)

  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_IGN;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */

  /*
   * Free memory...
   */
  if (Gmode == TEC_GMODE_TOPIX) {
    free(LastBuffer);
    free(CompBuffer);
  }
  free(Buffer);
}


/*
 * 'CancelJob()' - Cancel the current job...
 */
void
CancelJob(int sig)			/* I - Signal */
{
 /*
  * Tell the main loop to stop...
  */
  (void)sig;
  Canceled = 1;
}


/*
 * 'OutputLine()' - Output a line of graphics.
 * 
 * Some versions of this method check to see if the Buffer has data, this doesn't.
 * Empty lines can often be skipped if the buffer is checked.
 */
void
OutputLine(ppd_file_t           *ppd,	    /* I - PPD file */
           cups_page_header2_t  *header,	/* I - Page header */
           int                  y)	      /* I - Line number */
{

  if (Gmode == TEC_GMODE_TOPIX) {
    TOPIXCompress(ppd, header, y);
  } else {
    // Hex Output
    fwrite(Buffer, 1, header->cupsBytesPerLine, stdout);
  }

}



/*
 * 'TOPIXCompress()' - Apply TOPIX compression mechanism to current data in buffers
 */
void
TOPIXCompress(ppd_file_t         *ppd,	    /* I - PPD file */
              cups_page_header2_t *header,	/* I - Page header */
              int                y)         /* Line number */
{
  int               i;              /* Index into Buffer */
  int               max;            /* Max number of items per line */
  unsigned char     line[8][9][9] = {0};  /* Current line */
  int               l1, l2, l3;     /* Current Positions in line */ 
  unsigned char     cl1, cl2, cl3;  /* Current Characters */

  int               width;          /* Max width of the line */
  unsigned char     xor;      /* Current XORed character */
  unsigned char     *ptr;     /* Pointer into the Compressed Line Buffer */
 

  width = header->cupsBytesPerLine;
  max = 8 * 9 * 9;

  /*
   * Ensure that we will not overrun the buffer by sending 
   * to stdout when we get to the danger zone (width + ((width / 8) * 3))
   * This will create multiple graphics objects depending on the size of the image.
   */
  if ((CompBufferPtr - CompBuffer) > (0xFFFF - (width + (ceil(width / 8) * 3)))) {
    TOPIXCompressOutputBuffer(ppd, header, y);
    memset(LastBuffer, 0, header->cupsBytesPerLine);
  }

  /*
   * Perform XOR on raw data for TOPIX data
   */
  cl1 = 0;
  i = 0;
  for (l1 = 0; l1 <= 7 && i < width; l1++)
  {
    cl2 = 0;
    for (l2 = 1; l2 <= 8 && i < width; l2++)
    {
      cl3 = 0;
      for (l3 = 1; l3 <= 8 && i < width; l3++, i++)
      {
        xor = Buffer[i] ^ LastBuffer[i];
        line[l1][l2][l3] = xor;
        if (xor > 0) {
          // There is a change! Ensure its recorded
          cl3 |= (1 << (8 - l3));
        }
      } // L3

      line[l1][l2][0] = cl3;
      if (cl3 != 0)
        cl2 |= (1 << (8 - l2));
    } // L2

    line[l1][0][0] = cl2;
    if (cl2 != 0)
      cl1 |= (1 << (7 - l1));
  } // L1


  // Always add CL1 for line
  *CompBufferPtr = cl1;
  CompBufferPtr++;

  /*
   * Copy the line into the compressed buffer with all the
   * white space removed.
   */
  if (cl1 > 0) {
    ptr = &line[0][0][0];
    for(i = 0; i < max; i++) {
      if (*ptr != 0) {
        *CompBufferPtr = *ptr;
        CompBufferPtr++;
      }
      ptr++;
    }
  }
  
  /*
   * Copy line into last buffer ready for next loop
   */
  memcpy(LastBuffer, Buffer, header->cupsBytesPerLine);
}

/*
 * 'TOPIXCompressOutputBuffer()' - Send a set of data to output.
 *
 * Set y to 0 if this is the last line.
 */
void TOPIXCompressOutputBuffer(ppd_file_t          *ppd,	   /* PPD file */
                               cups_page_header2_t *header,	 /* Page header */
                               int                 y)        /* Line number */
{
  unsigned short len;
  unsigned short belen; /* Big-endian short! */

  len = (unsigned short) (CompBufferPtr - CompBuffer);
  if (len == 0)
    return;

  fprintf(stderr, "DEBUG: Sending output with length: %04x \n", len);

  // Convert into Big Endian (This may be OS dependant!)
  belen = (len << 8 | len >> 8);

  /*
   * Output the complete graphics line to STDOUT
   */
  printf("{SG;0000,%04d,%04d,%04d,%d,", CompLastLine, header->cupsBytesPerLine * 8, 300, Gmode);
  fwrite(&belen, 2, 1, stdout);       // Length of data
  fwrite(CompBuffer, 1, len, stdout); // Data
  printf("|}\n");
  fflush(stdout);

  if (y) CompLastLine = y;

  /*
   * Reset the Compressed Buffer
   */
  memset(CompBuffer, 0, 0xFFFF);
  CompBufferPtr = CompBuffer;
}



/*
 * 'main()' - Main entry and processing of driver.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int           			fd;		  /* File descriptor */
  cups_raster_t		    *ras;		/* Raster stream for printing */
  cups_page_header2_t	header;	/* Page header from file */
  int                 y;      /* Current line */
  ppd_file_t          *ppd;   /* PPD file */
  int                 num_options;	/* Number of options */
  cups_option_t       *options;	/* Options */


  /*
   * Make sure status messages are not buffered...
   */
  setbuf(stderr, NULL);

  /*
   * Check command-line...
   */
  if (argc < 6 || argc > 7)
  {
    /*
     * We don't have the correct number of arguments; write an error message
     * and return.
     */
    fputs("ERROR: rastertotec job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * Open the page stream...
  */
  if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) == -1)
    {
      perror("ERROR: Unable to open raster file - ");
      sleep(1);
      return (1);
    }
  }
  else
    fd = 0;

  ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

 /*
  * Open the PPD file and apply options...
  */
  num_options = cupsParseOptions(argv[5], 0, &options);

  if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL)
  {
    ppdMarkDefaults(ppd);
    cupsMarkOptions(ppd, num_options, options);
  }
  else
  {
    fputs("ERROR: Missing PPD file required for defaults!", stderr);
    return(1);
  }

  /*
   * Initialize the print device...
   */
  Setup(ppd);

  /*
   * Process pages as needed...
   */
  Page     = 0;
  Canceled = 0;

  while (cupsRasterReadHeader2(ras, &header))
  {
    /*
     * Write a status message with the page number and number of copies.
     */
    Page++;
    fprintf(stderr, "PAGE: %d 1\n", Page);

    /*
     * Start the page...
     */
    StartPage(ppd, &header);

    /*
     * Loop for each line on the page...
     */
    for (y = 0; y < header.cupsHeight && !Canceled; y++)
    {
      /*
       * Let the user know how far we have progressed...
       */
      if ((y & 15) == 0)
        fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", Page,
	        100 * y / header.cupsHeight);

      /*
       * Read a line of graphics...
       */
      if (cupsRasterReadPixels(ras, Buffer, header.cupsBytesPerLine) < 1)
        break;

      /*
       * Write it to the printer...
       */
      OutputLine(ppd, &header, y);
    }

    /*
     * Eject the page...
     */
    EndPage(ppd, &header);
    if (Canceled)
      break;
  }

  /*
   * Close the raster stream...
   */
  cupsRasterClose(ras);
  if (fd != 0)
    close(fd);

  /*
   * Close the PPD file and free the options...
   */
  ppdClose(ppd);
  cupsFreeOptions(num_options, options);

  /*
   * If no pages were printed, send an error message...
   */
  if (Page == 0)
    fputs("ERROR: No pages found!\n", stderr);
  else
    fputs("INFO: Ready to print.\n", stderr);
  return (Page == 0);
}

