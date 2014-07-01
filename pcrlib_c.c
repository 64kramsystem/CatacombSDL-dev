/* The Catacomb Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define CATALOG

#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>

#include "catdefs.h"
#include "catacomb.h"
#include "pcrlib.h"

char	ch,str[80];	// scratch space

/*========================================================================*/

inputtype playermode[3] = {0,keyboard,joystick1};

boolean		keydown[SDL_NUM_SCANCODES];

int JoyXlow [3], JoyXhigh [3], JoyYlow [3], JoyYhigh [3];

int MouseSensitivity, MouseEvent;

int key[8],keyB1,keyB2;

char	demobuffer[5000];
char	*demoptr;
int	democount;
int	lastdemoval;		// so demo can be RLE compressed
enum demoenum indemo;

static SDL_Scancode lastkey = 0;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *sdltexture;
SDL_Rect updateRect;

/*=======================================================================*/

/*
=======================
=
= SetupKBD
= Clears the keydown array and installs the INT 9 ISR if it isn't allready
= hooked up.
=
=======================
*/

void SetupKBD ()
{
 unsigned i;
 for (i=0;i<128;i++)			/* clear our key down table */
   keydown[i]= false;
}

static void ProcessEvents ()
{
	MouseEvent = 0;
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		if(event.type == SDL_KEYDOWN)
		{
			keydown[event.key.keysym.scancode] = true;
			lastkey = event.key.keysym.scancode;
		}
		else if(event.type == SDL_KEYUP)
		{
			keydown[event.key.keysym.scancode] = false;
		}
		else if(event.type == SDL_WINDOWEVENT)
		{
			if(event.window.event == SDL_WINDOWEVENT_CLOSE)
				_quit("");
		}
		else if(event.type == SDL_MOUSEMOTION)
		{
			MouseEvent = 1;
		}
	}
}

/*
===========================
=
= ControlKBD
=
===========================
*/

ControlStruct ControlKBD ()
{
 int xmove=0,
     ymove=0;
 ControlStruct action;

 if (keydown [key[north]])
  ymove=-1;
 if (keydown [key[east]])
  xmove=1;
 if (keydown [key[south]])
  ymove=1;
 if (keydown [key[west]])
  xmove=-1;

 if (keydown [key[northeast]])
 {
   ymove=-1;
   xmove=1;
 }
 if (keydown [key[northwest]])
 {
   ymove=-1;
   xmove=-1;
 }
 if (keydown [key[southeast]])
 {
   ymove=1;
   xmove=1;
 }
 if (keydown [key[southwest]])
 {
   ymove=1;
   xmove=-1;
 }

  switch (ymove*3+xmove)
 {
   case -4: action.dir = northwest; break;
   case -3: action.dir = north; break;
   case -2: action.dir = northeast; break;
   case -1: action.dir = west; break;
   case  0: action.dir = nodir; break;
   case  1: action.dir = east; break;
   case  2: action.dir = southwest; break;
   case  3: action.dir = south; break;
   case  4: action.dir = southeast; break;
 }

 action.button1 = keydown [keyB1];
 action.button2 = keydown [keyB2];

 return (action);
}


/*
============================
=
= ControlMouse
=
============================
*/

ControlStruct ControlMouse ()
{
 int newx,newy,		/* mickeys the mouse has moved */
     xmove = 0,
     ymove = 0;
 ControlStruct action;
 
 int buttons = SDL_GetMouseState(&newx, &newy);		/* mouse status */

 action.button1 = buttons & SDL_BUTTON(1);
 action.button2 = buttons & SDL_BUTTON(3);
 
 if (MouseEvent == 0)
 {
   action.dir = nodir;
   
   return (action);
 }

 if (newx-(320/2)>MouseSensitivity)
 {
   xmove = 1;
   newx = newx - MouseSensitivity*2;
 }
 else if (newx-(320/2)<-MouseSensitivity)
 {
   xmove = -1;
   newx = newx + MouseSensitivity*2;
 }
 if ((newy-100)>MouseSensitivity)
 {
   ymove = 1;
   newy = newy - MouseSensitivity;
 }
 else if ((newy-100)<-MouseSensitivity)
 {
   ymove = -1;
   newy = newy + MouseSensitivity;
 }

 newx = 160;
 newy = 100;
 SDL_WarpMouseInWindow(window, newx, newx);		/* set mouse status */

 switch (ymove*3+xmove)
 {
   case -4: action.dir = northwest; break;
   case -3: action.dir = north; break;
   case -2: action.dir = northeast; break;
   case -1: action.dir = west; break;
   case  0: action.dir = nodir; break;
   case  1: action.dir = east; break;
   case  2: action.dir = southwest; break;
   case  3: action.dir = south; break;
   case  4: action.dir = southeast; break;
 }

 return (action);
}


/*
===============================
=
= ReadJoystick
= Just return the resistance count of the joystick
=
===============================
*/

void ReadJoystick (int joynum,int *xcount,int *ycount)
{
	printf("STUB: %s\n", __FUNCTION__);
	*xcount = INT_MAX;
	*ycount = INT_MAX;

#ifdef NOTYET
 int portval,a1,a2,xbit,ybit;

 if (joynum==1)
 {
  xbit=1;
  ybit=2;
 }
 else
 {
  xbit=4;
  ybit=8;
 }

 *xcount = 0;
 *ycount = 0;

 outportb (0x201,inportb (0x201));	/* start the signal pulse */

 asm cli;

 do
 {
   portval = inportb (0x201);
   a1 = (portval & xbit) != 0;
   a2 = (portval & ybit) != 0;
   *xcount+=a1;
   *ycount+=a2;
 } while ((a1+a2!=0) && (*xcount<500) && (*ycount<500));

 asm sti;
#endif
}


/*
=============================
=
= ControlJoystick (joy# = 1 / 2)
=
=============================
*/

ControlStruct ControlJoystick (int joynum)
{
	FIXME
#ifdef NOTYET
 int joyx = 0,joyy = 0,		/* resistance in joystick */
     xmove = 0,
     ymove = 0,
     buttons;
 ControlStruct action;

 ReadJoystick (joynum,&joyx,&joyy);
 if ( (joyx>500) | (joyy>500) )
 {
   joyx=JoyXlow [joynum] + 1;	/* no joystick connected, do nothing */
   joyy=JoyYlow [joynum] + 1;
 }

 if (joyx > JoyXhigh [joynum])
   xmove = 1;
 else if (joyx < JoyXlow [joynum])
   xmove = -1;
 if (joyy > JoyYhigh [joynum])
   ymove = 1;
 else if (joyy < JoyYlow [joynum])
   ymove = -1;

 switch (ymove*3+xmove)
 {
   case -4: action.dir = northwest; break;
   case -3: action.dir = north; break;
   case -2: action.dir = northeast; break;
   case -1: action.dir = west; break;
   case  0: action.dir = nodir; break;
   case  1: action.dir = east; break;
   case  2: action.dir = southwest; break;
   case  3: action.dir = south; break;
   case  4: action.dir = southeast; break;
 }

 buttons = inportb (0x201);	/* get all four button status */
 if (joynum == 1)
 {
   action.button1 = ((buttons & 0x10) == 0);
   action.button2 = ((buttons & 0x20) == 0);
 }
 else
 {
   action.button1 = ((buttons & 0x40) == 0);
   action.button2 = ((buttons & 0x80) == 0);
 }
 return (action);
#endif
}


/*
=============================
=
= ControlPlayer
=
= Expects a 1 or a 2
=
=============================
*/

ControlStruct ControlPlayer (int player)
{
 ControlStruct ret;
 int val;

 ProcessEvents();
 if (indemo == notdemo || indemo == recording)
 {
   switch (playermode[player])
   {
     case keyboard : ret = ControlKBD (); break;
     case mouse    : ret = ControlMouse (); break;
     case joystick1: ret = ControlJoystick(1); break;
     case joystick2: ret = ControlJoystick(2); break;
   }

   //
   // recording a demo?
   //
   if (indemo == recording)
   {
     val = (ret.dir << 2) | (ret.button2 << 1) | ret.button1;
     *demoptr++=val;
   }


 }

 else

 //
 // get the command from the demo buffer
 //
 {
   val = *demoptr++;

   ret.button1 = val & 1;
   ret.button2 = (val & 2) >> 1;
   ret.dir = (dirtype) ( (val & (4+8+16+32) ) >> 2);
 }

 return (ret);
}


////////////////////////
//
// RecordDemo
// Clears the demo buffer and starts capturing events
//
////////////////////////

void RecordDemo (void)
{
  demobuffer[0]=level;
  demoptr = &demobuffer[1];
  indemo = recording;
}


////////////////////////
//
// LoadDemo / SaveDemo
// Loads a demo from disk or
// saves the accumulated demo command string to disk
//
////////////////////////

void LoadDemo (int demonum)
{
  char st2[5];

  strcpy (str,"DEMO");
  itoa (demonum,st2,10);
  strcat (str,st2);
  strcat (str,".");
  strcat (str,_extension);

  LoadFile (str,demobuffer);
  level=demobuffer[0];
  demoptr = &demobuffer[1];
  indemo = demoplay;
}

void SaveDemo (int demonum)
{
  char st2[5];

  strcpy (str,"DEMO");
  itoa (demonum,st2,10);
  strcat (str,st2);
  strcat (str,".");
  strcat (str,_extension);

  SaveFile (str,demobuffer,(demoptr-&demobuffer[0]));
  indemo = notdemo;
}


////////////////////////
//
// StartDemo
//
////////////////////////

/*=========================================================================*/


/*
** Miscellanious library routines
*/


///////////////////////////////
//
// clearkeys
// Clears out the bios buffer and zeros out the keydown array
//
///////////////////////////////

void clearkeys (void)
{
  int i;
  while (bioskey (1))
    bioskey(0);

  for (i=0;i<128;i++)
    keydown [i]=0;
}

//==========================================================================
#ifndef _WIN32
static long filelength (int fd)
{
	struct stat s;
	if(fstat(fd, &s))
		return -1;

	return s.st_size;
}
#endif

/*
==============================================
=
= Load a *LARGE* file into a FAR buffer!
= by John Romero (C) 1990 PCRcade
=
==============================================
*/

unsigned long LoadFile(char *filename,char *buffer)
{
	int fd;
	if((fd = open(filename, S_IREAD)) < 0)
		return 0;

	long len = filelength(fd);
	ssize_t bytesRead = read(fd, buffer, len);

	close(fd);
	return bytesRead;
}

//===========================================================================

/*
==============================================
=
= Save a *LARGE* file far a FAR buffer!
= by John Romero (C) 1990 PCRcade
=
==============================================
*/

void SaveFile(char *filename,char *buffer, long size)
{
	int fd;
	if((fd = open(filename, S_IWRITE)) < 0)
		return;

	write(fd, buffer, size);

	close(fd);
}

//==========================================================================

/*
====================================
=
= bloadin
= Paraligns just enough space and bloads in the
= specified file, returning a pointer to the start
=
====================================
*/

void *bloadin (char *filename)
{
 int handle;
 long length;
 char *location;

 if ( (handle = open (filename,O_BINARY)) != -1 )
   {
    length = filelength (handle);
    location = malloc (length);
    close (handle);
    LoadFile (filename,location);
    return location;
   }
 else
   return NULL;
}


/*==================================================================================*/

/*
** Graphic routines
*/

grtype grmode;

void *charptr;		// 8*8 tileset
void *tileptr;		// 16*16 tileset
void *picptr;		// any size picture set
void *spriteptr;		// any size masked and hit rect sprites
dword egaplaneofs[4];

unsigned crtcaddr;

int sx,sy,leftedge;

/*
========================
=
= setscreenmode
= Call BIOS to set TEXT / CGAgr / EGAgr / VGAgr
=
========================
*/

void setscreenmode (grtype mode)
{
	printf("STUB: %s\n", __FUNCTION__);

#ifdef NOTYET
  char extern VGAPAL;			// deluxepaint vga pallet .OBJ file
  void far *vgapal = &VGAPAL;

  switch (mode)
  {
    case text: _AX = 3;
	       geninterrupt (0x10);
	       screenseg=0xb800;
	       break;
    case CGAgr: _AX = 4;
		geninterrupt (0x10);
		screenseg=0xb800;
		break;
    case EGAgr: _AX = 0xd;
		geninterrupt (0x10);
		screenseg=0xa000;
		//EGAmove ();
		//moveega ();
		break;
    case VGAgr: _AX = 0x13;
		geninterrupt (0x10);
		screenseg=0xa000;
		_ES = FP_SEG(vgapal);
		_DX = FP_OFF(vgapal);
		_BX = 0;
		_CX = 0x100;
		_AX = 0x1012;
		geninterrupt(0x10);			// set the deluxepaint pallet
		break;
  }
  crtcaddr = 0x3d4;		//peek (0x40,0x63) if not for two monitors...
#endif
}

int win_xl,win_yl,win_xh,win_yh;

int screencenterx = 19,screencentery = 11;

//////////////////////////
//
// drawwindow
// draws a bordered window and homes the cursor
//
//////////////////////////

void drawwindow (int xl, int yl, int xh, int yh)
{
 int x,y;
 win_xl=xl;
 win_yl=yl;
 win_xh=xh;
 win_yh=yh;		// so the window can be erased

 drawchar (xl,yl,1);
 for (x=xl+1;x<xh;x++)
   drawchar (x,yl,2);
 drawchar (xh,yl,3);
 for (y=yl+1;y<yh;y++)
 {
   drawchar (xl,y,4);
   for (x=xl+1;x<xh;x++)
     drawchar (x,y,' ');
   drawchar (xh,y,5);
 }
 drawchar (xl,yh,6);
 for (x=xl+1;x<xh;x++)
   drawchar (x,yh,7);
 drawchar (xh,yh,8);

 sx = leftedge = xl+1;
 sy = yl+1;
}

////////////////////////////
//
// erasewindow
// clears out the last window and it's border to spaces
//
///////////////////////////

void bar (int xl, int yl, int xh, int yh, int ch)
{
  int x,y;

  for (y=yl;y<=yh;y++)
    for (x=xl;x<=xh;x++)
      drawchar (x,y,ch);
}


void erasewindow (void)
{
  bar (win_xl,win_yl,win_xh,win_yh,' ');
}

/////////////////////////////
//
// centerwindow
// Centers a drawwindow of the given size
//
/////////////////////////////

void centerwindow (int width, int height)
{
  int xl = screencenterx-width/2;
  int yl = screencentery-height/2;

  drawwindow (xl,yl,xl+width+1,yl+height+1);
}

///////////////////////////////
//
// expwin {h / v}
// Grows the window outward
//
///////////////////////////////
void expwin (int width, int height)
{
  if (width > 2)
  {
    if (height >2)
      expwin (width-2,height-2);
    else
      expwinh (width-2,height);
  }
  else
    if (height >2)
      expwinv (width,height-2);

  UpdateScreen();
  WaitVBL ();
  centerwindow (width,height);
}

void expwinh (int width, int height)
{
  if (width > 2)
    expwinh (width-2,height);

  UpdateScreen();
  WaitVBL ();
  centerwindow (width,height);
}

void expwinv (int width, int height)
{
  if (height >2)
    expwinv (width,height-2);

  UpdateScreen();
  WaitVBL ();
  centerwindow (width,height);
}


/////////////////////////
//
// get
// Flash a cursor at sx,sy and waits for a user bioskey
//
/////////////////////////

int bioskey(int cmd)
{
	if(lastkey)
	{
		int oldkey = lastkey;
		if(cmd != 1)
			lastkey = 0;
		return oldkey;
	}

	SDL_Event event;
	while (SDL_PollEvent (&event))
	{
		if(event.type == SDL_KEYDOWN)
		{
			if(cmd == 1)
				return lastkey = event.key.keysym.scancode;
			return event.key.keysym.scancode;
		}
		else if(event.type == SDL_WINDOWEVENT)
		{
			if(event.window.event == SDL_WINDOWEVENT_CLOSE)
				_quit("");
		}
	}
	return lastkey;
}

void UpdateScreen()
{
	static Uint32 EGAPalette[16] = {
		0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
		0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
		0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
		0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
	};
	static Uint32 CGAPalette[4] = {
		0x000000, 0x55FFFF, 0xFF55FF, 0xFFFFFF
	};
	static Uint32 conv[sizeof(screenseg)];

	size_t i = 0;
	if(grmode == EGAgr)
	{
		while (i < sizeof(screenseg))
		{
			conv[i] = EGAPalette[screenseg[i]];
			++i;
		}
	}
	else if(grmode == CGAgr)
	{
		while (i < sizeof(screenseg))
		{
			conv[i] = CGAPalette[screenseg[i]];
			++i;
		}
	}
	else
		assert(false && "VGA Palette conversion not implemented.");

	SDL_UpdateTexture(sdltexture, NULL, conv, 320*sizeof(Uint32));
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, sdltexture, NULL, &updateRect);
	SDL_RenderPresent(renderer);
}

int get (void)
{
 SDL_Event event;

 int cycle,key;

 do
 {
   cycle = 9;
   while (!(key = bioskey(0)) && cycle<13)
   {
     drawchar (sx,sy,cycle++);
	 UpdateScreen();
     WaitVBL ();
     WaitVBL ();
     WaitVBL ();
     WaitVBL ();
     WaitVBL ();
   }
 } while (key == 0);
 drawchar (sx,sy,' ');
 return SDL_GetKeyFromScancode(key);		// take it out of the buffer
}


/////////////////////////
//
// print
// Prints a string at sx,sy.  No clipping!!!
//
/////////////////////////

void print (const char *str)
{
  char ch;

  while ((ch=*str++) != 0)
    if (ch == '\n')
    {
      sy++;
      sx=leftedge;
    }
    else if (ch == '\r')
      sx=leftedge;
    else
      drawchar (sx++,sy,(byte)ch);
}

// For help screen
void printchartile (const char *str)
{
  char ch;

  while ((ch=*str++) != 0)
    if (ch == '\n')
    {
      sy++;
      sx=leftedge;
    }
    else if (ch == '\r')
      sx=leftedge;
    else
      drawchartile (sx++,sy,(byte)ch);
}


///////////////////////////
//
// printint / printlong
// Converts the value to a string and prints it
//
///////////////////////////

void printint (int val)
{
  itoa(val,str,10);
  print (str);
}

void printlong (long val)
{
  ltoa(val,str,10);
  print (str);
}

/*========================================================================*/

int _MouseStatus;

////////////////////////////////////////////////////////////////////
//
// Mouse Routines
//
////////////////////////////////////////////////////////////////////
int _MouseInit(void)
{
	FIXME
#ifdef NOTYET
 union REGS regs;
 unsigned char far *vector;

 if ((vector=MK_FP(peek(0,0x33*4+2),peek(0,0x33*4)))==NULL) return 0;

 if (*vector == 207)
   return _MouseStatus = 0;

 _AX=0;
 geninterrupt(0x33);
 return _MouseStatus = 1;
#endif
}



void _MouseHide(void)
{
	FIXME
#ifdef NOTYET
 if (!_MouseStatus) return;

 _AX=2;
 geninterrupt(0x33);
#endif
}



void _MouseShow(void)
{
	FIXME
#ifdef NOTYET
 if (!_MouseStatus) return;

 _AX=1;
 geninterrupt(0x33);
#endif
}



int _MouseButton(void)
{
	FIXME
#ifdef NOTYET
 union REGS regs;

 if (!_MouseStatus) return 0;

 regs.x.ax=3;
 int86(0x33,&regs,&regs);
 return(regs.x.bx);
#endif
}



void _MouseCoords(int *x,int *y)
{
	FIXME
#ifdef NOTYET
 union REGS regs;

 if (!_MouseStatus) return;

 regs.x.ax=3;
 int86(0x33,&regs,&regs);
 *x=regs.x.cx;
 *y=regs.x.dx;

 *x/=2;
#endif
}




////////////////////////////////////////////////////////////////////
//
// Verify a file's existence
//
////////////////////////////////////////////////////////////////////
long _Verify(char *filename)
{
 int handle;
 long size;

 if ((handle=open(filename,O_BINARY))==-1) return 0;
 size=filelength(handle);
 close(handle);
 return size;
}





////////////////////////////////////////////////////////////////////
//
// print hex byte
//
////////////////////////////////////////////////////////////////////
void _printhexb(unsigned char value)
{
 int loop;
 char hexstr[16]="0123456789ABCDEF",str[2]="";

 for (loop=0;loop<2;loop++)
   {
    str[0]=hexstr[(value>>(1-loop)*4)&15];
    print(str);
   }
}




////////////////////////////////////////////////////////////////////
//
// print hex
//
////////////////////////////////////////////////////////////////////
void _printhex(unsigned value)
{
 print("$");
 _printhexb(value>>8);
 _printhexb(value&0xff);
}




////////////////////////////////////////////////////////////////////
//
// print bin
//
////////////////////////////////////////////////////////////////////
void _printbin(unsigned value)
{
 int loop;

 print("%");
 for (loop=0;loop<16;loop++)
    if ((value>>15-loop)&1) print("1"); else print("0");
}




////////////////////////////////////////////////////////////////////
//
// center print
//
////////////////////////////////////////////////////////////////////
void _printc(char *string)
{
 sx=1+screencenterx-(strlen(string)/2);
 print(string);
}




////////////////////////////////////////////////////////////////////
//
// input unsigned
//
////////////////////////////////////////////////////////////////////
unsigned _inputint(void)
{
 char string[18]="",digit,hexstr[16]="0123456789ABCDEF";
 unsigned value,loop,loop1;

 _input(string,17);
 if (string[0]=='$')
   {
    int digits;

    digits=strlen(string)-2;
    if (digits<0) return 0;

    for (value=0,loop1=0;loop1<=digits;loop1++)
      {
       digit=toupper(string[loop1+1]);
       for (loop=0;loop<16;loop++)
	  if (digit==hexstr[loop])
	    {
	     value|=(loop<<(digits-loop1)*4);
	     break;
	    }
      }
   }
 else if (string[0]=='%')
   {
    int digits;

    digits=strlen(string)-2;
    if (digits<0) return 0;

    for (value=0,loop1=0;loop1<=digits;loop1++)
      {
       if (string[loop1+1]<'0' || string[loop1+1]>'1') return 0;
       value|=(string[loop1+1]-'0')<<(digits-loop1);
      }
   }
 else value=atoi(string);
 return value;
}




////////////////////////////////////////////////////////////////////
//
// line input routine
//
////////////////////////////////////////////////////////////////////
int _input(char *string,int max)
{
 char key;
 int count=0,loop;

 do {
     key=toupper(get()&0xff);
     if ((key==127 || key==8)&&count>0)
       {
	count--;
	drawchar(sx,sy,' ');
	sx--;
       }

     if (key>=' ' && key<='z' && count<max)
       {
	*(string+count++)=key;
	drawchar(sx++,sy,key);
       }

    } while (key!=27 && key!=13);

 for (loop=count;loop<max;loop++) *(string+loop)=0;

 if (key==13) return 1;
 return 0;
}

/*========================================================================*/

/*
** Game routines
*/

struct scores scoreswap, highscores[5];

sdword score;
sword level;
int _numlevels, _maxplayers;

char *_extension = "PCR";
boolean	_cgaok = true, _egaok = true, _vgaok = false;

static const SDL_Scancode DOSScanCodeMap[128] = {
	0,SDL_SCANCODE_ESCAPE,SDL_SCANCODE_1,SDL_SCANCODE_2,SDL_SCANCODE_3,
	SDL_SCANCODE_4,SDL_SCANCODE_5,SDL_SCANCODE_6,SDL_SCANCODE_7,SDL_SCANCODE_8,
	SDL_SCANCODE_9,SDL_SCANCODE_0,SDL_SCANCODE_MINUS,SDL_SCANCODE_EQUALS,SDL_SCANCODE_BACKSPACE,
	SDL_SCANCODE_TAB,SDL_SCANCODE_Q,SDL_SCANCODE_W,SDL_SCANCODE_E,SDL_SCANCODE_R,
	SDL_SCANCODE_T,SDL_SCANCODE_Y,SDL_SCANCODE_U,SDL_SCANCODE_I,SDL_SCANCODE_O,
	SDL_SCANCODE_P,SDL_SCANCODE_LEFTBRACKET,SDL_SCANCODE_RIGHTBRACKET,SDL_SCANCODE_RETURN,SDL_SCANCODE_LCTRL,
	SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,SDL_SCANCODE_F,SDL_SCANCODE_G,
	SDL_SCANCODE_H,SDL_SCANCODE_J,SDL_SCANCODE_K,SDL_SCANCODE_L,SDL_SCANCODE_SEMICOLON,
	SDL_SCANCODE_APOSTROPHE,SDL_SCANCODE_GRAVE,SDL_SCANCODE_LSHIFT,SDL_SCANCODE_BACKSLASH,SDL_SCANCODE_Z,
	SDL_SCANCODE_X,SDL_SCANCODE_C,SDL_SCANCODE_V,SDL_SCANCODE_B,SDL_SCANCODE_N,SDL_SCANCODE_M,
	SDL_SCANCODE_COMMA,SDL_SCANCODE_PERIOD,SDL_SCANCODE_SLASH,SDL_SCANCODE_RSHIFT,SDL_SCANCODE_SYSREQ,
	SDL_SCANCODE_LALT,SDL_SCANCODE_SPACE,SDL_SCANCODE_CAPSLOCK,SDL_SCANCODE_F1,SDL_SCANCODE_F2,
	SDL_SCANCODE_F3,SDL_SCANCODE_F4,SDL_SCANCODE_F5,SDL_SCANCODE_F6,SDL_SCANCODE_F7,
	SDL_SCANCODE_F8,SDL_SCANCODE_F9,SDL_SCANCODE_F10,SDL_SCANCODE_NUMLOCKCLEAR,SDL_SCANCODE_SCROLLLOCK,
	SDL_SCANCODE_HOME,SDL_SCANCODE_UP,SDL_SCANCODE_PAGEUP,SDL_SCANCODE_KP_MINUS,SDL_SCANCODE_LEFT,
	SDL_SCANCODE_KP_5,SDL_SCANCODE_RIGHT,SDL_SCANCODE_KP_PLUS,SDL_SCANCODE_END,SDL_SCANCODE_DOWN,
	SDL_SCANCODE_PAGEDOWN,SDL_SCANCODE_INSERT,SDL_SCANCODE_DELETE,0,0,
	0,SDL_SCANCODE_F11,SDL_SCANCODE_F12
};

static int ScancodeToDOS(SDL_Scancode sc)
{
	int i = 0;
	for(i = 0;i < 128;++i)
	{
		if(DOSScanCodeMap[i] == sc)
			return i;
	}
	return 0;
}

#pragma pack(1)
typedef struct
{
	word grmode;
	word soundmode;
	word playermode[3];
	sword JoyXlow[3];
	sword JoyYlow[3];
	sword JoyXhigh[3];
	sword JoyYhigh[3];
	sword MouseSensitivity;
	byte key[8];
	byte keyB1;
	byte keyB2;
} ctlpaneltype;
#pragma pack()

////////////////////////
//
// _loadctrls
// Tries to load the control panel settings
// creates a default if not present
//
////////////////////////

void _loadctrls (void)
{
  int handle;

  strcpy (str,"CTLPANEL.");
  strcat (str,_extension);
  if ((handle = open(str, O_RDONLY | O_BINARY, S_IWRITE | S_IREAD)) == -1)
  //
  // set up default control panel settings
  //
  {
    grmode=VGAgr;
    soundmode=spkr;
    playermode[1] = keyboard;
    playermode[2] = joystick1;

    JoyXlow [1] = JoyXlow [2] = 20;
    JoyXhigh[1] = JoyXhigh[2] = 60;
    JoyYlow [1] = JoyYlow [2] = 20;
    JoyYhigh[1] = JoyYhigh[2] = 60;
    MouseSensitivity = 5;

    key[north] = SDL_SCANCODE_UP;
    key[northeast] = SDL_SCANCODE_PAGEUP;
    key[east] = SDL_SCANCODE_RIGHT;
    key[southeast] = SDL_SCANCODE_PAGEDOWN;
    key[south] = SDL_SCANCODE_DOWN;
    key[southwest] = SDL_SCANCODE_END;
    key[west] = SDL_SCANCODE_LEFT;
    key[northwest] = SDL_SCANCODE_HOME;
    keyB1 = SDL_SCANCODE_LCTRL;
    keyB2 = SDL_SCANCODE_LALT;
  }
  else
  {
	ctlpaneltype ctlpanel;
	read(handle, &ctlpanel, sizeof(ctlpanel));
	close(handle);

	grmode = ctlpanel.grmode;
	soundmode = ctlpanel.soundmode;
	unsigned i;
	for(i = 0;i < 3;++i)
	{
		playermode[i] = ctlpanel.playermode[i];
		JoyXlow[i] = ctlpanel.JoyXlow[i];
		JoyYlow[i] = ctlpanel.JoyYlow[i];
		JoyXhigh[i] = ctlpanel.JoyXhigh[i];
		JoyYhigh[i] = ctlpanel.JoyYhigh[i];
		
		if (playermode[i] == mouse)
		{
			SDL_SetRelativeMouseMode(SDL_TRUE);
		}
	}
	MouseSensitivity = ctlpanel.MouseSensitivity;
	for(i = 0;i < 8;++i)
		key[i] = DOSScanCodeMap[ctlpanel.key[i]];
	keyB1 = DOSScanCodeMap[ctlpanel.keyB1];
	keyB2 = DOSScanCodeMap[ctlpanel.keyB2];
  }
}

void _savectrls (void)
{
  int handle;

  strcpy (str,"CTLPANEL.");
  strcat (str,_extension);

  if ((handle = open(str, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE)) == -1)
    return;

  ctlpaneltype ctlpanel;
  ctlpanel.grmode = grmode;
  ctlpanel.soundmode = soundmode;
  unsigned i;
  for(i = 0;i < 3;++i)
  {
	ctlpanel.playermode[i] = playermode[i];
	ctlpanel.JoyXlow[i] = JoyXlow[i];
	ctlpanel.JoyYlow[i] = JoyYlow[i];
	ctlpanel.JoyXhigh[i] = JoyXhigh[i];
	ctlpanel.JoyYhigh[i] = JoyYhigh[i];
  }
  ctlpanel.MouseSensitivity = MouseSensitivity;
  for(i = 0;i < 8;++i)
	  ctlpanel.key[i] = ScancodeToDOS(key[i]);
  ctlpanel.keyB1 = ScancodeToDOS(keyB1);
  ctlpanel.keyB2 = ScancodeToDOS(keyB2);

  write(handle, &ctlpanel, sizeof(ctlpanel));

  close(handle);
}


////////////////////////
//
// loadhighscores
// Tries to load the score file
// creates a default if not present
//
////////////////////////
void _loadhighscores (void)
{
  int i;

  strcpy (str,"SCORES.");
  strcat (str,_extension);
  if (LoadFile(str,(char *)highscores) == 0 )
    for (i=0;i<5;i++)
    {
      highscores[i].score = 100;
      highscores[i].level = 1;
      strcpy(highscores[i].initials,"PCR");
    }
}

void _savehighscores (void)
{
  strcpy (str,"SCORES.");
  strcat (str,_extension);
  SaveFile(str,(char *)highscores,sizeof (highscores));
}


////////////////////////
//
// _showhighscores
// Brings up a dialog box with the high score lists and returns immediately
//
////////////////////////
void _showhighscores (void)
{
  int i;
  long h;
  char st2[10];

  centerwindow (17,17);
  print ("\n   HIGH SCORES\n\n");
  print (" #  SCORE LV  BY\n");
  print (" - ------ -- ---\n");
  for (i=0;i<5;i++)
  {
    sx++;
    drawchar (sx,sy,'1'+i);
    sx+=2;
    h=highscores[i].score;
    if (h<100000l)
      sx++;
    if (h<10000l)
      sx++;
    if (h<1000l)
      sx++;
    if (h<100l)
      sx++;
    if (h<10l)
      sx++;
    ltoa(h,str,10);
    print (str);
    sx++;
    if (highscores[i].level<10)
      sx++;
    itoa(highscores[i].level,str,10);
    print (str);
    sx++;
    print (highscores[i].initials);
    print ("\n\n");
  }
  strcpy (str,"SCORE:");
  ltoa (score,st2,10);
  strcat (str,st2);

  _printc (str);
}


//////////////////////////
//
// _checkhighscore
// Compares score to highscores, and inserts place if needed.
// calls showhighscores in any case
//
//////////////////////////
void _checkhighscore (void)
{
  int i,j,k;

  for (i=0;i<5;i++)
    if (score>highscores[i].score)
    {
      for (j=4;i<j;j--)
      {
	k=j-1;
	highscores[j] = highscores[k];
      }
      highscores[i].score = score;
      highscores[i].level = level;
      strcpy(highscores[i].initials,"   ");
      break;
    }

  _showhighscores ();
  UpdateScreen ();

  //
  // did get a high score
  //
  if (i<5)
  {
    PlaySound (16);
    clearkeys ();
    sx = screencenterx-17/2+14;
    sy = screencentery-17/2+6+i*2;
    j=0;
    do
    {
      ch = k = get();
      if (ch>=' ' && j<3)
      {
	drawchar (sx,sy,ch);
	sx++;
	highscores[i].initials[j]=ch;
	j++;
      }
      if (ch==8 || k==19200)
	if (j>0)
	{
	  sx--;
	  j--;
	}
    } while (ch != 13);
  }
}


////////////////////
//
// _setupgame
//
////////////////////

void SetupEmulatedVBL();
byte screenseg[320*200];

static char* VideoParmStrings[] = {"windowed", "screen", 0};

void _setupgame (void)
{
  if (SDL_Init (SDL_INIT_VIDEO|SDL_INIT_TIMER))
  {
    fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
    exit(1);
  }

  int i;
  boolean windowed = false;
  unsigned winWidth = 640, winHeight = 480;
  int displayindex = 0;
  for (i = 1;i < _argc; ++i)
  {
    switch (US_CheckParm (_argv[i],VideoParmStrings))
    {
    case 0:
      windowed = true;
      if (++i < _argc)
        winWidth = atoi (_argv[i]);
      if (++i < _argc)
        winHeight = atoi (_argv[i]);
        break;
    case 1:
      if (++i < _argc)
        displayindex = atoi (_argv[i]);
      break;
    }
  }

  SDL_DisplayMode mode;
  SDL_Rect bounds;
  if (SDL_GetCurrentDisplayMode (displayindex, &mode) < -1 ||
      SDL_GetDisplayBounds (displayindex, &bounds) < 0)
  {
    fprintf(stderr, "Could not get display mode: %s\n", SDL_GetError());
    exit(1);
  }

  if (windowed)
  {
    mode.w = winWidth;
    mode.h = winHeight;
  }

  if ((window = SDL_CreateWindow ("The Catacomb", bounds.x, bounds.y, mode.w, mode.h, windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP)) == NULL ||
      (renderer = SDL_CreateRenderer (window, -1, 0)) == NULL)
  {
    fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
    exit(1);
  }

  if (!(sdltexture = SDL_CreateTexture (renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200)))
  {
    fprintf(stderr, "Could not create video buffer: %s\n", SDL_GetError());
    exit(1);
  }

  // Handle 320x200 and 640x400 specially so they are unscaled.
  if ((mode.w == 320 && mode.h == 200) || (mode.w == 640 && mode.h == 400))
  {
    updateRect.w = mode.w;
    updateRect.h = mode.h;
    updateRect.x = updateRect.y = 0;
  }
  else
  {
    // Pillar box the 4:3 game
    updateRect.h = mode.h;
    updateRect.w = mode.h*4/3;
    updateRect.x = (mode.w - updateRect.w)>>1;
    updateRect.y = 0;
  }

  memset(screenseg, 0, sizeof(screenseg));

//
// set up game's library routines
//
  grmode = EGAgr;

  // allways assume CGA compatability for simCGA garbage

  _loadctrls ();

  if (grmode==VGAgr && _vgaok)
    grmode=VGAgr;
  else if (grmode>=EGAgr && _egaok)
    grmode=EGAgr;
  else
    grmode=CGAgr;

  strcpy (str,"SOUNDS.");
  strcat (str,_extension);

  SoundData = (SPKRtable *) bloadin (str);

  StartupSound ();

  SetupKBD ();

  initrndt (1);		// setup random routines
  initrnd (1);		// original rng

  _loadhighscores ();

  loadgrfiles ();	// load the graphic files

  SetupEmulatedVBL();
}


////////////////////
//
// _quit
//
////////////////////

void _quit (char *error)
{
  setscreenmode (text);
  if (!(*error))
  {
	 _savehighscores ();
	 _savectrls ();
  }
  else
  {
	puts (error);
	puts("\n");
	puts("\n");
	puts("For techinical assistance with running this software\n");
	puts("    call Softdisk Publishing at 1-318-221-8311\n");
	puts("\n");
	exit(1);
  }

  ShutdownSound ();

#ifndef CATALOG
	_argc = 2;
	_argv[1] = "LAST.SHL";
	_argv[2] = "ENDSCN.SCN";
	_argv[3] = NULL;
	if (execv("LOADSCN.EXE", _argv) == -1)
		_quit("Couldn't find executable LOADSCN.EXE.\n");
#endif



//  movedata (FP_SEG(&PIRACY),FP_OFF(&PIRACY),0xb800,0,4000);

//  clearkeys ();

//  bioskey (0);

//  clrscr ();

  exit (0);		// quit to DOS
}


