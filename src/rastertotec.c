/*
 * "$Id: rastertotec.c 6236 2007-02-05 21:04:04Z mike $"
 *
 *   Toshiba TEC Label printer filter for the Common UNIX Printing System (CUPS).
 *
 *   Original version by Patrick Kong, 2009-2010.
 * 
 *   Copyright 2001-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   Setup()        - Prepare the printer for printing.
 *   StartPage()    - Start a page of graphics.
 *   EndPage()      - Finish a page of graphics.
 *   CancelJob()    - Cancel the current job...
 *   OutputLine()   - Output a line of graphics. modification P.kong 21/07/2009 for tec printers
 *   TPCLCompress() - Output a run-length compression sequence for TEC printers.
 *   main()         - Main entry and processing of driver.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
// #include <cups/string.h>
#include <cups/raster.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


/*
 * This driver filter currently supports Toshiba TEC Label Printers
 *
 * The basic code layout is the same as that in the CUPS rastertolabel.
 */

/*
 * Model number constants...
 */

#define TEC_TPCL2	0x14		/* Toshiba Tec TPCL2 based printers */
#define INTSIZE		20 			/* MAXIMUM CHARACTERS INTEGER */

/*
 * Globals...
 */
unsigned char	*Buffer;		/* Output buffer */
unsigned char	*CompBuffer;		/* Compression buffer */
unsigned char	*LastBuffer;		/* Last buffer */
int		LastSet;		/* Number of repeat characters */
int		ModelNumber,		/* cupsModelNumber attribute */
		Page,			/* Current page */
		Feed,			/* Number of lines to skip */
		Canceled;		/* Non-zero if job is canceled */
char		Xsprint[25];		/* Final command for tec print string 2009 p.kong*/
int 		Gmode; 			/* Tec Graphics mode pkong 2009*/

/*
 * Prototypes...
 */
void	Setup(ppd_file_t *ppd);
void	StartPage(ppd_file_t *ppd, cups_page_header_t *header);
void	EndPage(ppd_file_t *ppd, cups_page_header_t *header);
void	CancelJob(int sig);
void	OutputLine(ppd_file_t *ppd, cups_page_header_t *header, int y);
void	TPCLCompress(char repeat_char, int repeat_count);


/*
 * 'Setup()' - Prepare the printer for printing.
 */

void
Setup(ppd_file_t *ppd)			/* I - PPD file */
{
  char		*Fadjm;			/* Fine adjust printing position */
  char		*Radj;			/* Ribbon adjust parameter */
  ppd_choice_t	*choice;		/* Marked choice */
  /* initialize Fadjm */
  Fadjm = (char *) malloc(INTSIZE +2); /* Advanced parameters for printer */
  Radj  = (char *) malloc(INTSIZE +2); /* Ribbon ajust parameter */

  /*
   * Get the model number from the PPD file...
   */
  if (ppd)
    ModelNumber = ppd->model_number;
  
  /*
   * Initialize based on the model number...
   */

  switch (ModelNumber)
  {
    case TEC_TPCL2 :
      /*  
       * Modification to take in consideration feed ajust reverse feed etc
       */
      strcpy(Fadjm,"{AX;\0"); /* Place command start */
      /* feed adjust */
      choice = ppdFindMarkedChoice(ppd, "FAdjSgn");
    	switch (atoi(choice->choice))
    	{
      	case 0 :
        	strcat(Fadjm,"+\0");
        	break;
       	case 1 :
        	strcat(Fadjm,"-\0");
        	break;
      	default :
        	strcat(Fadjm,"+\0");
        	break;
     	}
      choice = ppdFindMarkedChoice(ppd, "FAdjV");
    	strcat(Fadjm,choice->choice);
      
      /* Cut adjust peel adjust */
      choice = ppdFindMarkedChoice(ppd, "CAdjSgn");
    	switch (atoi(choice->choice))
      {
        case 0 :
          strcat(Fadjm,",+\0");
        	break;
      	case 1 :
        	strcat(Fadjm,",-\0");
        	break;
      	default :
        	strcat(Fadjm,",+\0");
        	break;
      }
      choice = ppdFindMarkedChoice(ppd, "CAdjV");
      strcat(Fadjm,choice->choice);

      /* back feed adjust */
      choice = ppdFindMarkedChoice(ppd, "RAdjSgn");
	    switch (atoi(choice->choice))
    	{
       	case 0 :
         	strcat(Fadjm,",+\0");
        	break;
      	case 1 :
        	strcat(Fadjm,",-\0");
        	break;
       	default :
        	strcat(Fadjm,",+\0");
        	break;
     	}

    	choice = ppdFindMarkedChoice(ppd, "RAdjV");
	    strcat(Fadjm,choice->choice);
      
      /* close the command */
	    strcat(Fadjm,"|}\n\0");
      /* send the command */
    	puts(Fadjm); /*send advanced parameters */

      /* Send ribbon Motor setup parameters */
	    strcpy(Radj,"{RM;\0");	/* start command for ribbon */
    	choice = ppdFindMarkedChoice(ppd, "RbnAdjFwd");
    	strcat(Radj,choice->choice); /* value for take up motor */
    	choice = ppdFindMarkedChoice(ppd, "RbnAdjBck");
    	strcat(Radj,choice->choice);
    	strcat(Radj,"|}\n\0");
    	puts(Radj);
      break;

  } // Switch ModelNumber
}


/*
 * 'StartPage()' - Start a page of graphics.
 */
void
StartPage(ppd_file_t         *ppd,	/* I - PPD file */
          cups_page_header_t *header)	/* I - Page header */
{
  ppd_choice_t  *choice;		/* Marked choice */
  int         	length;			/* Actual label length */
  int           Labelgap;		/* length of labelgap */
  int 		      Width;			/* page with in mm */
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
  fprintf(stderr, "DEBUG: PageSize = [ %d %d ]\n", header->PageSize[0],
          header->PageSize[1]);
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

  switch (ModelNumber)
  { 
    case TEC_TPCL2  :
      // printf("{XJ;Page Start|}");
    	/*
       * First paper size Dxxxx,xxxx,xxxx then
       * Start bitmap graphics...
       */

    	Gmode =1;  /* default graphics drawing mode */
      choice = ppdFindMarkedChoice(ppd,"TeGraphicsMode");
      if (atoi(choice->choice) == 1)
        Gmode = 1;
      else if (atoi(choice->choice) == 2)
        Gmode = 5; 

      /* Get labelgap for printing */
      choice = ppdFindMarkedChoice(ppd, "Gap");
      Labelgap = atoi(choice->choice)*10;
      length = header->PageSize[1]*254/72 + Labelgap;
      Width = header->PageSize[0]*254/72;

      /* Send label size */
      printf("{D%04d,%04d,%04d|}\n",length,Width,header->PageSize[1]*254/72); 
	    printf("{C|}\n"); 	/*clear print buffer */

      /*
       * Place the right command in the parameter AY temperature fine adjust
       */
      switch (header->cupsCompression)
      {
        case 1 :
          strcpy(Fadjt,"{AY;-10,\0");
        	break;
        case 2 :
          strcpy(Fadjt,"{AY;-09,\0");
          break;
        case 3 :
          strcpy(Fadjt,"{AY;-08,\0");
          break;
        case 4 :
          strcpy(Fadjt,"{AY;-07,\0");
          break;
        case 5 :
          strcpy(Fadjt,"{AY;-06,\0");
          break;
        case 6 :
          strcpy(Fadjt,"{AY;-05,\0");
          break;
        case 7 :
          strcpy(Fadjt,"{AY;-04,\0");
          break;
        case 8 :
          strcpy(Fadjt,"{AY;-03,\0");
          break;
        case 9 :
          strcpy(Fadjt,"{AY;-02,\0");
          break;
        case 10 :
          strcpy(Fadjt,"{AY;-01,\0");
          break;
        case 11 :
	        strcpy(Fadjt,"{AY;+00,\0");
          break;
        case 12 :
          strcpy(Fadjt,"{AY;+01,\0");
          break;
        case 13 :
          strcpy(Fadjt,"{AY;+02,\0");
          break;
        case 14 :
          strcpy(Fadjt,"{AY;+03,\0");
          break;
        case 15 :
          strcpy(Fadjt,"{AY;+04,\0");
          break;
        case 16 :
          strcpy(Fadjt,"{AY;+05,\0");
	        break;
        case 17 :
          strcpy(Fadjt,"{AY;+06,\0");
	        break;
        case 18 :
          strcpy(Fadjt,"{AY;+07,\0");
          break;
        case 19 :
          strcpy(Fadjt,"{AY;+08,\0");
          break;
        case 20 :
          strcpy(Fadjt,"{AY;+09,\0");
          break;
        case 21 :
          strcpy(Fadjt,"{AY;+10,\0");
          break;
      }
      
      /*
       * Completing fine adjust according to Thermal or direct printing
       */
      if (!strcmp(header->MediaType, "Thermal"))
        strcat(Fadjt,"1|}\0");
      else if (!strcmp(header->MediaType, "Direct"))
        strcat(Fadjt,"0|}\n\0");
      
      /*
       * Send parameter to printer
       */
      puts(Fadjt);

      /* Get graphics mode from ppd file for graphics drawing */
      printf("{SG;0000,0000,%04d,%04d,%d,",header->cupsBytesPerLine * 8,header->cupsHeight,Gmode);
      
      /*
       * Allocate buffers for 8 dots per byte graphics
       */
	    CompBuffer = malloc(header->cupsBytesPerLine + 1);
      LastBuffer = malloc(header->cupsBytesPerLine);
	    LastSet    = 0;
	    break;
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
        cups_page_header_t *header)	/* I - Page header */
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
  switch (ModelNumber)
  {    
      case TEC_TPCL2  :
        if (Canceled)
        {
	        /*
           * Ramclear in case of error
           */
          puts("{WR|}\n");
          break;
        }

        /*
         * Start label...
         */
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
        * Set media type...
        */
       if (!strcmp(header->MediaType, "Thermal"))
         Tmedia = 2;
       else if (!strcmp(header->MediaType, "Direct"))
         Tmedia = 0;
       else if (!strcmp(header->MediaType,"Thermal2"))
         Tmedia = 1;
       else if (strcmp(header->MediaType,"Direct2"))
         Tmedia = 3;
       
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

      /* 
       * send print command 
       * Free compression buffers...
       */
      free(CompBuffer);
      free(LastBuffer);

  } // end switch ModelNumber

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
 * 'OutputLine()' - Output a line of graphics...
 */
void
OutputLine(ppd_file_t         *ppd,	/* I - PPD file */
           cups_page_header_t *header,	/* I - Page header */
           int                y)	/* I - Line number */
{
  // int            i;			      /* Looping var */
  // unsigned char	*ptr;         /* Pointer into buffer */
  // unsigned char	*compptr;     /* Pointer into compression buffer */
  // char		        repeat_char;  /* Repeated character */
  // int		        repeat_count; /* Number of repeated characters */
  // static const char *hex = "0123456789ABCDEF"; /* Hex digits */

  switch (ModelNumber)
  {
    
    case TEC_TPCL2 :
      /* 
       *  Read the buffer and send directly in hexmode 8 bits.
       *  In case of sending the page as an image to the printer added by p.kong S.K.E sarl 2009
       */ 
      if (Buffer[0] || memcmp(Buffer, Buffer + 1, header->cupsBytesPerLine))
      {
        fwrite(Buffer, 1, header->cupsBytesPerLine, stdout);
        fflush(stdout);
      }
      break;
  } // switch ModelNumber
}


/*
 * 'TPCLCompress()' - Output a run-length compression sequence.
 */
void
TPCLCompress(char repeat_char,		/* I - Character to repeat */
	           int  repeat_count)		/* I - Number of repeated characters */
{
  if (repeat_count > 1)
  {
    /*
     * Print as many z's as possible - they are the largest denomination
     * representing 400 characters (zC stands for 400 adjacent C's)	
     */	
    while (repeat_count >= 400)
    {
      putchar('z');
      repeat_count -= 400;
    }

    /*
     * Then print 'g' through 'y' as multiples of 20 characters...
     */
    if (repeat_count >= 20)
    {
      putchar('f' + repeat_count / 20);
      repeat_count %= 20;
    }

    /*
     * Finally, print 'G' through 'Y' as 1 through 19 characters...
     */
    if (repeat_count > 0)
      putchar('F' + repeat_count);
  }

  /*
   * Then the character to be repeated...
   */
  putchar(repeat_char);
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
  cups_page_header_t	header;	/* Page header from file */
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

 /*
  * Initialize the print device...
  */
  Setup(ppd);

 /*
  * Process pages as needed...
  */

  Page      = 0;
  Canceled = 0;

  while (cupsRasterReadHeader(ras, &header))
  {
   /*
    * Write a status message with the page number and number of copies.
    */

    Page ++;

    fprintf(stderr, "PAGE: %d 1\n", Page);

   /*
    * Start the page...
    */

    StartPage(ppd, &header);

   /*
    * Loop for each line on the page...
    */

    for (y = 0; y < header.cupsHeight && !Canceled; y ++)
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
    * Close sending graphics if Tec
    */
    if (ModelNumber == TEC_TPCL2)
      printf("|}\n");

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


/*
 * End of "$Id: rastertotec.c 6236 2007-02-05 21:04:04Z mike $".
 */
