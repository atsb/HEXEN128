
//**************************************************************************
//**
//** f_finale.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: f_finale.c,v $
//** $Revision: 1.7 $
//** $Date: 96/01/05 23:33:26 $
//** $Author: bgokey $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "soundst.h"
#include "p_local.h"
#include <ctype.h>

// MACROS ------------------------------------------------------------------

#define	TEXTSPEED	3
#define	TEXTWAIT	250

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void TextWrite(void);
static void DrawPic(void);
static void InitializeFade(boolean fadeIn);
static void DeInitializeFade(void);
static void FadePic(void);
#if (APPVER_HEXENREV >= AV_HR_HEX11A)
static char *GetFinaleText(int sequence);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean automapactive;
extern boolean viewactive;

// PUBLIC DATA DECLARATIONS ------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int FinaleStage;
static int FinaleCount;
static int FinaleEndCount;
static int FinaleLumpNum;
static int FontABaseLump;
static char *FinaleText;

static fixed_t *Palette;
static fixed_t *PaletteDelta;
static byte *RealPalette;

#if (APPVER_HEXENREV < AV_HR_HEX11A)
static char *FinaleTexts[] =
{
	"with a scream of agony you are\n"
	"wrenched from this world into\n"
	"another, every part of your body\n"
	"wreathed in mystic fire.  when your\n"
	"vision clears, you find yourself\n"
	"standing in a great hall, filled\n"
	"with ghostly echoes and menacing\n"
	"shadows.  in the distance you can\n"
	"see a raised dais, and upon it the\n"
	"only source of light in this world.",

	"this can only be the chaos sphere,\n"
	"the source of korax's power.  with\n"
	"this, you can create worlds...  or\n"
	"destroy them.  by rights of battle\n"
	"and conquest it is yours, and with\n"
	"trembling hands you reach to grasp\n"
	"it.  perhaps, now, a new player will\n"
	"join the cosmic game of power.  like\n"
	"the pawn who is promoted to queen,\n"
	"suddenly the very reaches of the\n"
	"board seem to be within your grasp.",

	"                                    \n"
	"but there are other players mightier\n"
	"than you, and who can know their\n"
	"next moves?"
};
#endif // APPVER_HEXENREV

// CODE --------------------------------------------------------------------

//===========================================================================
//
// F_StartFinale
//
//===========================================================================

void F_StartFinale (void)
{
#if (APPVER_HEXENREV >= AV_HR_HEX10A) || (!defined APPVER_DEMO)
	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;
	automapactive = false;
#if (APPVER_HEXENREV < AV_HR_HEX10A) // From Heretic
	players[consoleplayer].messageTics = 1;
	players[consoleplayer].message = NULL;
#else
	P_ClearMessage(&players[consoleplayer]);
#endif

	FinaleStage = 0;
	FinaleCount = 0;
#if (APPVER_HEXENREV < AV_HR_HEX11A)
	FinaleText = FinaleTexts[0];
#else
	FinaleText = GetFinaleText(0);
#endif
	FinaleEndCount = 70;
	FinaleLumpNum = W_GetNumForName("FINALE1");
	FontABaseLump = W_GetNumForName("FONTA_S")+1;
	InitializeFade(1);

//	S_ChangeMusic(mus_victor, true);
	S_StartSongName("hall", false); // don't loop the song
#endif
}

//===========================================================================
//
// F_Responder
//
//===========================================================================

boolean F_Responder(event_t *event)
{
	return false;
}

//===========================================================================
//
// F_Ticker
//
//===========================================================================

void F_Ticker (void)
{
	FinaleCount++;
	if(FinaleStage < 5 && FinaleCount >= FinaleEndCount)
	{
		FinaleCount = 0;
		FinaleStage++;
		switch(FinaleStage)
		{
			case 1: // Text 1
				FinaleEndCount = strlen(FinaleText)*TEXTSPEED+TEXTWAIT;
				break;
			case 2: // Pic 2, Text 2
#if (APPVER_HEXENREV < AV_HR_HEX11A)
				FinaleText = FinaleTexts[1];
#else
				FinaleText = GetFinaleText(1);
#endif
				FinaleEndCount = strlen(FinaleText)*TEXTSPEED+TEXTWAIT;
				FinaleLumpNum = W_GetNumForName("FINALE2");
				S_StartSongName("orb", false);
				break;
			case 3: // Pic 2 -- Fade out
				FinaleEndCount = 70;
				DeInitializeFade();
				InitializeFade(0);
				break;
			case 4: // Pic 3 -- Fade in
				FinaleLumpNum = W_GetNumForName("FINALE3");
				FinaleEndCount = 71;
				DeInitializeFade();
				InitializeFade(1);
				S_StartSongName("chess", true);
				break;
			case 5: // Pic 3 , Text 3
#if (APPVER_HEXENREV < AV_HR_HEX11A)
				FinaleText = FinaleTexts[2];
#else
				FinaleText = GetFinaleText(2);
#endif
				DeInitializeFade();
				break;
			default:
				 break;
		}
		return;
	}
	if(FinaleStage == 0 || FinaleStage == 3 || FinaleStage == 4)
	{
		FadePic();
	}
}

//===========================================================================
//
// TextWrite
//
//===========================================================================

static void TextWrite (void)
{
	int		count;
	char	*ch;
	int		c;
	int		cx, cy;
	patch_t *w;

	memcpy(screen, W_CacheLumpNum(FinaleLumpNum, PU_CACHE), 
		SCREENWIDTH*SCREENHEIGHT);
	if(FinaleStage == 5)
	{ // Chess pic, draw the correct character graphic
		if(netgame)
		{
			V_DrawPatch(20, 0, W_CacheLumpName("chessall", PU_CACHE));
		}
		else if(PlayerClass[consoleplayer])
		{
			V_DrawPatch(60, 0, W_CacheLumpNum(W_GetNumForName("chessc")
				+PlayerClass[consoleplayer]-1, PU_CACHE));
		}
	}
	// Draw the actual text
	if(FinaleStage == 5)
	{
		cy = 135;
	}
	else
	{
		cy = 5;
	}
	cx = 20;
	ch = FinaleText;
	count = (FinaleCount-10)/TEXTSPEED;
	if (count < 0)
	{
		count = 0;
	}
	for(; count; count--)
	{
		c = *ch++;
		if(!c)
		{
			break;
		}
		if(c == '\n')
		{
			cx = 20;
			cy += 9;
			continue;
		}
#if (APPVER_HEXENREV < AV_HR_HEX11A)
		c = toupper(c);
		if(c < 33)
#else
		if(c < 32)
		{
			continue;
		}
		c = toupper(c);
		if(c == 32)
#endif
		{
			cx += 5;
			continue;
		}
		w = W_CacheLumpNum(FontABaseLump+c-33, PU_CACHE);
		if(cx+w->width > SCREENWIDTH)
		{
			break;
		}
		V_DrawPatch(cx, cy, w);
		cx += w->width;
	}
}

//===========================================================================
//
// InitializeFade
//
//===========================================================================

static void InitializeFade(boolean fadeIn)
{
	unsigned i;

	Palette = Z_Malloc(768*sizeof(fixed_t), PU_STATIC, 0);
	PaletteDelta = Z_Malloc(768*sizeof(fixed_t), PU_STATIC, 0);
	RealPalette = Z_Malloc(768*sizeof(byte), PU_STATIC, 0);

	if(fadeIn)
	{
		memset(RealPalette, 0, 768*sizeof(byte));
		for(i = 0; i < 768; i++)
		{
			Palette[i] = 0;
			PaletteDelta[i] = FixedDiv((*((byte *)W_CacheLumpName("playpal", 
				PU_CACHE)+i))<<FRACBITS, 70*FRACUNIT);
		}
	}
	else
	{
		for(i = 0; i < 768; i++)
		{
			RealPalette[i] = *((byte *)W_CacheLumpName("playpal", PU_CACHE)+i);
			Palette[i] = RealPalette[i]<<FRACBITS;
			PaletteDelta[i] = FixedDiv(Palette[i], -70*FRACUNIT);
		}
	}
	I_SetPalette(RealPalette);
}

//===========================================================================
//
// DeInitializeFade
//
//===========================================================================

static void DeInitializeFade(void)
{
	Z_Free(Palette);
	Z_Free(PaletteDelta);
	Z_Free(RealPalette);
}

//===========================================================================
//
// FadePic
//
//===========================================================================

static void FadePic(void)
{
	unsigned i;

	for(i = 0; i < 768; i++)
	{
		Palette[i] += PaletteDelta[i];
		RealPalette[i] = Palette[i]>>FRACBITS;
	}
	I_SetPalette(RealPalette);
}

//===========================================================================
//
// DrawPic
//
//===========================================================================

static void DrawPic(void)
{
	memcpy(screen, W_CacheLumpNum(FinaleLumpNum, PU_CACHE), 
		SCREENWIDTH*SCREENHEIGHT);
	if(FinaleStage == 4 || FinaleStage == 5)
	{ // Chess pic, draw the correct character graphic
		if(netgame)
		{
			V_DrawPatch(20, 0, W_CacheLumpName("chessall", PU_CACHE));
		}
		else if(PlayerClass[consoleplayer])
		{
			V_DrawPatch(60, 0, W_CacheLumpNum(W_GetNumForName("chessc")
				+PlayerClass[consoleplayer]-1, PU_CACHE));
		}
	}
}

//===========================================================================
//
// F_Drawer
//
//===========================================================================

void F_Drawer(void)
{
	switch(FinaleStage)
	{
		case 0: // Fade in initial finale screen
			DrawPic();
			break;
		case 1:
		case 2:
			TextWrite();
			break;
		case 3: // Fade screen out
			DrawPic();
			break;
		case 4: // Fade in chess screen
			DrawPic();
			break;
		case 5:
			TextWrite();
			break;
	}
	UpdateState |= I_FULLSCRN;	
}

#if (APPVER_HEXENREV >= AV_HR_HEX11A)
//==========================================================================
//
// GetFinaleText
//
//==========================================================================

static char *GetFinaleText(int sequence)
{
	char *msgLumpName;
	int msgSize;
	int msgLump;
	static char *winMsgLumpNames[] =
	{
		"win1msg",
		"win2msg",
		"win3msg"
	};

	msgLumpName = winMsgLumpNames[sequence];
	msgLump = W_GetNumForName(msgLumpName);
	msgSize = W_LumpLength(msgLump);
	if(msgSize >= MAX_INTRMSN_MESSAGE_SIZE)
	{
		I_Error("Finale message too long (%s)", msgLumpName);
	}
	W_ReadLump(msgLump, ClusterMessage);
	ClusterMessage[msgSize] = 0; // Append terminator
	return ClusterMessage;
}
#endif // APPVER_HEXENREV
