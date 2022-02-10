
//**************************************************************************
//**
//** in_lude.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: in_lude.c,v $
//** $Revision: 1.19 $
//** $Date: 96/01/05 23:33:19 $
//** $Author: bgokey $
//**
//**************************************************************************

#include "h2def.h"
#include <ctype.h>

// MACROS ------------------------------------------------------------------

#define	TEXTSPEED 3
#define	TEXTWAIT 140

// TYPES -------------------------------------------------------------------

typedef enum
{
	SINGLE,
	COOPERATIVE,
	DEATHMATCH
} gametype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void IN_Start(void);
void IN_Ticker(void);
void IN_Drawer(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void WaitStop(void);
static void Stop(void);
static void LoadPics(void);
static void UnloadPics(void);
static void CheckForSkip(void);
static void InitStats(void);
static void DrDeathTally(void);
#if (APPVER_HEXENREV < AV_HR_HEX11A)
static void IN_DrawNumber(int val, int x, int y, int digits);
static void IN_DrTextB(char *text, int x, int y);
static void IN_DrTextC(char *str, int x, int y);
#else
static void DrNumber(int val, int x, int y, int wrapThresh);
static void DrNumberBold(int val, int x, int y, int wrapThresh);
#endif
static void DrawHubText(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

boolean intermission;
#if (APPVER_HEXENREV >= AV_HR_HEX11A)
char ClusterMessage[MAX_INTRMSN_MESSAGE_SIZE];
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean skipintermission;
static int interstate = 0;
static int intertime = -1;
static gametype_t gametype;
static int cnt;
static int slaughterboy; // in DM, the player with the most kills
static patch_t *patchINTERPIC;
static patch_t *FontBNumbers[10];
static patch_t *FontBNegative;
static patch_t *FontBSlash;
static patch_t *FontBPercent;
static int FontABaseLump;
static int FontBLump;
static int FontBLumpBase;

static signed int totalFrags[MAXPLAYERS];

static int HubCount;
static char *HubText;

// CODE --------------------------------------------------------------------

//========================================================================
//
// IN_Start
//
//========================================================================

extern void AM_Stop (void);

void IN_Start(void)
{
	int i;
	I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
#if (APPVER_HEXENREV < AV_HR_HEX10A) // From Heretic
	LoadPics();
	InitStats();
#else
	InitStats();
	LoadPics();
#endif
	intermission = true;
	interstate = 0;
	skipintermission = false;
	intertime = 0;
	AM_Stop();
	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].messageTics = 0;
#if (APPVER_HEXENREV < AV_HR_HEX10A)
		players[i].message = 0;
#else
		players[i].message[0] = 0;
#endif
	}
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA)
	S_StartSongName("hub", true);
#else
	SN_StopAllSequences();	
#endif
}

//========================================================================
//
// WaitStop
//
//========================================================================

void WaitStop(void)
{
	if(!--cnt)
	{
		Stop();
//		gamestate = GS_LEVEL;
//		G_DoLoadLevel();
		gameaction = ga_leavemap;
//		G_WorldDone();
	}
}

//========================================================================
//
// Stop
//
//========================================================================

static void Stop(void)
{
	intermission = false;
	UnloadPics();
	SB_state = -1;
	BorderNeedRefresh = true;
}

//========================================================================
//
// InitStats
//
// 	Initializes the stats for single player mode
//========================================================================

#if (APPVER_HEXENREV < AV_HR_HEX11A)
static char *PlayerKillersString = "KILLERS";

static char *ClusMsgs[] =
{
	"having passed the seven portals\n"
	"which sealed this realm, a vast\n"
	"domain of harsh wilderness stretches\n"
	"before you. fire, ice and steel have\n"
	"tested you, but greater challenges\n"
	"remain ahead. the dense tangle of\n"
	"forest surely hides hostile eyes,\n"
	"but what lies beyond will be worse.\n"
	"\n"
	"barren desert, dank swamps and\n"
	"musty caverns bar your way, but you\n"
	"cannot let anything keep you from\n"
	"your fate, even if you might come\n"
	"to wish that it would.\n"
	"\n"
	"and beyond, flickering in the\n"
	"distance, the ever-shifting walls\n"
	"of the hypostyle seem to mock\n"
	"your every effort.\n",

	"your mind still reeling from your\n"
	"encounters within the hypostyle, you\n"
	"stagger toward what you hope is\n"
	"a way out. things seem to move faster\n"
	"and faster, your vision blurs and\n"
	"begins to fade...\n"
	"as the world collapses around you,\n"
	"the brightness of a teleportal\n"
	"engulfs you. a flash of light, and then\n"
	"you climb wearily to your feet.\n"
	"\n"
	"you stand atop a high tower, and\n"
	"from below come the screams of the\n"
	"damned. you step forward, and\n"
	"instantly the sound of demonic\n"
	"chanting chills your blood.\n"
	"by all the gods of death! what place\n"
	"have you come to? by all the gods of\n"
	"pain, how will you ever find your\n"
	"way out?",

	"the mightiest weapons and artifacts\n"
	"of the ancients barely sufficed to\n"
	"defeat the heresiarch and his\n"
	"minions, but now their foul remains\n"
	"lie strewn at your feet. gathering\n"
	"the last of your strength, you\n"
	"prepare to enter the portal which\n"
	"leads from the heresiarch's inner\n"
	"sanctum.\n"
	"\n"
	"above you, the ramparts of an\n"
	"immense castle loom. silent towers\n"
	"and bare walls surround a single\n"
	"spire of black stone, which squats\n"
	"in the center of the castle like a\n"
	"brooding giant. fire and shadow\n"
	"twist behind gaping windows, dozens\n"
	"of baleful eyes glaring down upon\n"
	"you.\n"
	"somewhere within, your enemies are\n"
	"waiting...",

	"\"... and he shall journey into the\n"
	"realms of the dead, and contest with\n"
	"the forces therein, unto the very\n"
	"gates of despair. but whether he\n"
	"shall return again to the world of\n"
	"light, no man knows.\"\n"
	"                    \n"
	"                    \n"
	"                    \n"
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	"damn.\n"
	"                    \n"
	"                    \n"
	"damn damn damn damn damn damn damn."
#else
	"                    \n"
	"                    \n"
	"damn.\n"
#endif
};
#else // APPVER_HEXENREV >= AV_HR_HEX11A
static char *ClusMsgLumpNames[] =
{
	"clus1msg",
	"clus2msg",
	"clus3msg",
	"clus4msg",
	"clus5msg"
};
#endif

static void InitStats(void)
{
	int i;
	int j;
	int oldCluster;
	signed int slaughterfrags;
	int posnum;
	int slaughtercount;
	int playercount;
#if (APPVER_HEXENREV >= AV_HR_HEX11A)
	char *msgLumpName;
	int msgSize;
	int msgLump;
#endif

	extern int LeaveMap;

	if(!deathmatch)
	{
		gametype = SINGLE;
		HubCount = 0;
		oldCluster = P_GetMapCluster(gamemap);
		if(oldCluster != P_GetMapCluster(LeaveMap))
		{
			if(oldCluster >= 1 && oldCluster <= 5)
			{
#if (APPVER_HEXENREV >= AV_HR_HEX11A)
				msgLumpName = ClusMsgLumpNames[oldCluster-1];
				msgLump = W_GetNumForName(msgLumpName);
				msgSize = W_LumpLength(msgLump);
				if(msgSize >= MAX_INTRMSN_MESSAGE_SIZE)
				{
					I_Error("Cluster message too long (%s)", msgLumpName);
				}
				W_ReadLump(msgLump, ClusterMessage);
				ClusterMessage[msgSize] = 0; // Append terminator
				HubText = ClusterMessage;
#else
				HubText = ClusMsgs[oldCluster-1];
#endif
				HubCount = strlen(HubText)*TEXTSPEED+TEXTWAIT;
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
				S_StartSongName("hub", true);
#endif
			}
		}
	}
	else
	{
		gametype = DEATHMATCH;
		slaughterboy = 0;
		slaughterfrags = -9999;
		posnum = 0;
		playercount = 0;
		slaughtercount = 0;
		for(i=0; i<MAXPLAYERS; i++)
		{
			totalFrags[i] = 0;
			if(playeringame[i])
			{
				playercount++;
				for(j=0; j<MAXPLAYERS; j++)
				{
					if(playeringame[j])
					{
						totalFrags[i] += players[i].frags[j];
					}
				}
				posnum++;
			}
			if(totalFrags[i] > slaughterfrags)
			{
				slaughterboy = 1<<i;
				slaughterfrags = totalFrags[i];
				slaughtercount = 1;
			}
			else if(totalFrags[i] == slaughterfrags)
			{
				slaughterboy |= 1<<i;
				slaughtercount++;
			}
		}
		if(playercount == slaughtercount)
		{ // don't do the slaughter stuff if everyone is equal
			slaughterboy = 0;
		}
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
		S_StartSongName("hub", true);
#endif
	}
}

//========================================================================
//
// LoadPics
//
//========================================================================

static void LoadPics(void)
{
	int i;

#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	if(HubCount || gametype == DEATHMATCH)
#endif
	{
		patchINTERPIC = W_CacheLumpName("INTERPIC", PU_STATIC);
		FontBLumpBase = W_GetNumForName("FONTB16");
		for(i=0; i<10; i++)
		{
			FontBNumbers[i] = W_CacheLumpNum(FontBLumpBase+i, PU_STATIC);
		}
		FontBLump = W_GetNumForName("FONTB_S")+1;
		FontBNegative = W_CacheLumpName("FONTB13", PU_STATIC);
		FontABaseLump = W_GetNumForName("FONTA_S")+1;
	
		FontBSlash = W_CacheLumpName("FONTB15", PU_STATIC);
		FontBPercent = W_CacheLumpName("FONTB05", PU_STATIC);
	}
}

//========================================================================
//
// UnloadPics
//
//========================================================================

static void UnloadPics(void)
{
	int i;

#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	if(HubCount || gametype == DEATHMATCH)
#endif
	{
		Z_ChangeTag(patchINTERPIC, PU_CACHE);
		for(i=0; i<10; i++)
		{
			Z_ChangeTag(FontBNumbers[i], PU_CACHE);
		}
		Z_ChangeTag(FontBNegative, PU_CACHE);
		Z_ChangeTag(FontBSlash, PU_CACHE);
		Z_ChangeTag(FontBPercent, PU_CACHE);
	}
}

//========================================================================
//
// IN_Ticker
//
//========================================================================

void IN_Ticker(void)
{
	if(!intermission)
	{
		return;
	}
	if(interstate)
	{
		WaitStop();
		return;
	}
	skipintermission = false;
	CheckForSkip();
	intertime++;
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	if(skipintermission || (!deathmatch && intertime > HubCount))
#else
	if(skipintermission || (gametype == SINGLE && !HubCount))
#endif
	{
		interstate = 1;
		cnt = 10;
		skipintermission = false;
		//S_StartSound(NULL, sfx_dorcls);
	}
}

//========================================================================
//
// CheckForSkip
//
// 	Check to see if any player hit a key
//========================================================================

static void CheckForSkip(void)
{
  	int i;
	player_t *player;
	static boolean triedToSkip;

  	for(i = 0, player = players; i < MAXPLAYERS; i++, player++)
	{
    	if(playeringame[i])
    	{
			if(player->cmd.buttons&BT_ATTACK)
			{
				if(!player->attackdown)
				{
					skipintermission = 1;
				}
				player->attackdown = true;
			}
			else
			{
				player->attackdown = false;
			}
			if(player->cmd.buttons&BT_USE)
			{
				if(!player->usedown)
				{
					skipintermission = 1;
				}
				player->usedown = true;
			}
			else
			{
				player->usedown = false;
			}
		}
	}
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	if(intertime < 140)
#else
	if(deathmatch && intertime < 140)
#endif
	{ // wait for 4 seconds before allowing a skip
		if(skipintermission == 1)
		{
			triedToSkip = true;
			skipintermission = 0;
		}
	}
	else
	{
		if(triedToSkip)
		{
			skipintermission = 1;
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
			triedToSkip = false;
#endif
		}
	}
#endif // APPVER_HEXENREV >= AV_HR_HEXDEMOA
}

//========================================================================
//
// IN_Drawer
//
//========================================================================

void IN_Drawer(void)
{
	if(!intermission)
	{
		return;
	}
	if(interstate)
	{
		return;
	}
	UpdateState |= I_FULLSCRN;
	memcpy(screen, (byte *)patchINTERPIC, SCREENWIDTH*SCREENHEIGHT);

	if(gametype == SINGLE)
	{
		if(HubCount)
		{
			DrawHubText();
		}
	}
	else
	{
		DrDeathTally();
	}
}

//========================================================================
//
// DrDeathTally
//
//========================================================================

#if (APPVER_HEXENREV < AV_HR_HEX11A) // Mostly from Heretic

static char *PlayerColors[] = {"BLUE", "RED", "YELLOW", "GREEN"};

static void DrDeathTally(void)
{
	int i;
	int j;
	int ypos;
	int xpos;
	int kpos;

	static int sounds;

	xpos = 68;
	ypos = 50;

	IN_DrTextB("TOTAL", 270, 27);
	IN_DrTextB("VICTIMS", 125, 8);
	for(i=0; i<7; i++)
	{
		IN_DrTextC(PlayerKillersString, 7, 44);
	}
	if(intertime < 20)
	{
		sounds = 0;
		return;
	}
	if(intertime >= 20 && sounds < 1)
	{
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
		S_StartSound(NULL, SFX_PLATFORM_STOP);
#endif
		sounds++;
	}
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA)
	if (intertime >= 100 && slaughterboy && sounds < 2)
	{
		sounds++;
	}
#endif
	for(i=0; i<MAXPLAYERS; i++)
	{
		if(playeringame[i])
		{
			kpos = 50;
			for(j=0; j<MAXPLAYERS; j++)
			{
				if(playeringame[j])
				{
					IN_DrawNumber(players[i].frags[j], kpos, ypos, 3);
					kpos += 50;
				}
			}
#if (APPVER_HEXENREV < AV_HR_HEX10A)
			if(slaughterboy&(1<<i))
			{
				if(!(intertime&16))
				{
					IN_DrawNumber(totalFrags[i], 263, ypos, 3);
				}
			}
			else
			{
				IN_DrawNumber(totalFrags[i], 263, ypos, 3);
			}
#else
			if(!((slaughterboy&(1<<i)) && (intertime&16)))
				IN_DrawNumber(totalFrags[i], 263, ypos, 3);
#endif
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
			if(i == consoleplayer)
			{
				MN_DrTextAYellow(PlayerColors[i], xpos, 35);
				MN_DrTextAYellow(PlayerColors[i], 28, ypos+5);
			}
			else
#endif
			{
				MN_DrTextA(PlayerColors[i], xpos, 35);
				MN_DrTextA(PlayerColors[i], 28, ypos+5);
			}
			xpos += 50;
			ypos += 30;
		}
	}
}

#if (APPVER_HEXENREV < AV_HR_HEX10A) // From Heretic
//========================================================================
//
// IN_DrawTime
//
//========================================================================

void IN_DrawTime(int x, int y, int h, int m, int s)
{
	if(h)
	{
		IN_DrawNumber(h, x, y, 2);
		IN_DrTextB(":", x+26, y);
	}
	x += 34;
	if(m || h)
	{
		IN_DrawNumber(m, x, y, 2);
	}
	x += 34;
	if(s)
	{
		IN_DrTextB(":", x-8, y);
		IN_DrawNumber(s, x, y, 2);
	}
}
#endif // APPVER_HEXENREV < AV_HR_HEX10A

//========================================================================
//
// IN_DrawNumber
//
//========================================================================

static void IN_DrawNumber(int val, int x, int y, int digits)
{
	patch_t *patch;
	int xpos;
	int oldval;
	int realdigits;
	boolean neg;

	oldval = val;
	xpos = x;
	neg = false;
	realdigits = 1;

	if(val < 0)
	{ //...this should reflect negative frags
		val = -val;
		neg = true;
		if(val > 99)
		{
			val = 99;
		}
	}
	if(val > 9)
	{
		realdigits++;
		if(digits < realdigits)
		{
			realdigits = digits;
			val = 9;
		}
	}
	if(val > 99)
	{
		realdigits++;
		if(digits < realdigits)
		{
			realdigits = digits;
			val = 99;
		}
	}
	if(val > 999)
	{
		realdigits++;
		if(digits < realdigits)
		{
			realdigits = digits;
			val = 999;
		}
	}
	if(digits == 4)
	{
		patch = FontBNumbers[val/1000];
		V_DrawShadowedPatch(xpos+6-patch->width/2-12, y, patch);
	}
	if(digits > 2)
	{
		if(realdigits > 2)
		{
			patch = FontBNumbers[val/100];
			V_DrawShadowedPatch(xpos+6-patch->width/2, y, patch);
		}
		xpos += 12;
	}
	val = val%100;
	if(digits > 1)
	{
		if(val > 9)
		{
			patch = FontBNumbers[val/10];
			V_DrawShadowedPatch(xpos+6-patch->width/2, y, patch);
		}
		else if(digits == 2 || oldval > 99)
		{
			V_DrawShadowedPatch(xpos, y, FontBNumbers[0]);
		}
		xpos += 12;
	}
	val = val%10;
	patch = FontBNumbers[val];
	V_DrawShadowedPatch(xpos+6-patch->width/2, y, patch);
	if(neg)
	{
		patch = FontBNegative;
		V_DrawShadowedPatch(xpos+6-patch->width/2-12*(realdigits), y, patch);
	}
}

//========================================================================
//
// IN_DrTextB
//
//========================================================================

static void IN_DrTextB(char *text, int x, int y)
{
	char c;
	patch_t *p;

	while((c = *text++) != 0)
	{
		if(c < 33)
		{
			x += 8;
		}
		else
		{
			p = W_CacheLumpNum(FontBLump+c-33, PU_CACHE);
			V_DrawShadowedPatch(x, y, p);
			x += p->width-1;
		}
	}
}

#else // APPVER_HEXENREV

#define TALLY_EFFECT_TICKS 20
#define TALLY_FINAL_X_DELTA (23*FRACUNIT)
#define TALLY_FINAL_Y_DELTA (13*FRACUNIT)
#define TALLY_START_XPOS (178*FRACUNIT)
#define TALLY_STOP_XPOS (90*FRACUNIT)
#define TALLY_START_YPOS (132*FRACUNIT)
#define TALLY_STOP_YPOS (83*FRACUNIT)
#define TALLY_TOP_X 85
#define TALLY_TOP_Y 9
#define TALLY_LEFT_X 7
#define TALLY_LEFT_Y 71
#define TALLY_TOTALS_X 291

static void DrDeathTally(void)
{
	int i, j;
	fixed_t xPos, yPos;
	fixed_t xDelta, yDelta;
	fixed_t xStart, scale;
	int x, y;
	boolean bold;
	static boolean showTotals;
	int temp;

	V_DrawPatch(TALLY_TOP_X, TALLY_TOP_Y,
		W_CacheLumpName("tallytop", PU_CACHE));
	V_DrawPatch(TALLY_LEFT_X, TALLY_LEFT_Y,
		W_CacheLumpName("tallylft", PU_CACHE));
	if(intertime < TALLY_EFFECT_TICKS)
	{
		showTotals = false;
		scale = (intertime*FRACUNIT)/TALLY_EFFECT_TICKS;
		xDelta = FixedMul(scale, TALLY_FINAL_X_DELTA);
		yDelta = FixedMul(scale, TALLY_FINAL_Y_DELTA);
		xStart = TALLY_START_XPOS-FixedMul(scale,
			TALLY_START_XPOS-TALLY_STOP_XPOS);
		yPos = TALLY_START_YPOS-FixedMul(scale,
			TALLY_START_YPOS-TALLY_STOP_YPOS);
	}
	else
	{
		xDelta = TALLY_FINAL_X_DELTA;
		yDelta = TALLY_FINAL_Y_DELTA;
		xStart = TALLY_STOP_XPOS;
		yPos = TALLY_STOP_YPOS;
	}
	if(intertime >= TALLY_EFFECT_TICKS && showTotals == false)
	{
		showTotals = true;
		S_StartSound(NULL, SFX_PLATFORM_STOP);
	}
	y = yPos>>FRACBITS;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		xPos = xStart;
		for(j = 0; j < MAXPLAYERS; j++, xPos += xDelta)
		{
			x = xPos>>FRACBITS;
			bold = (i == consoleplayer || j == consoleplayer);
			if(playeringame[i] && playeringame[j])
			{
				if(bold)
				{
					DrNumberBold(players[i].frags[j], x, y, 100);
				}
				else
				{
					DrNumber(players[i].frags[j], x, y, 100);
				}
			}
			else
			{
				temp = MN_TextAWidth("--")/2;
				if(bold)
				{
					MN_DrTextAYellow("--", x-temp, y);
				}
				else
				{
					MN_DrTextA("--", x-temp, y);
				}
			}
		}
		if(showTotals && playeringame[i]
			&& !((slaughterboy&(1<<i)) && !(intertime&16)))
		{
			DrNumber(totalFrags[i], TALLY_TOTALS_X, y, 1000);
		}
		yPos += yDelta;
		y = yPos>>FRACBITS;
	}
}

//==========================================================================
//
// DrNumber
//
//==========================================================================

static void DrNumber(int val, int x, int y, int wrapThresh)
{
	char buff[8] = "XX";

	if(!(val < -9 && wrapThresh < 1000))
	{
		sprintf(buff, "%d", val >= wrapThresh ? val%wrapThresh : val);
	}
	MN_DrTextA(buff, x-MN_TextAWidth(buff)/2, y);
}

//==========================================================================
//
// DrNumberBold
//
//==========================================================================

static void DrNumberBold(int val, int x, int y, int wrapThresh)
{
	char buff[8] = "XX";

	if(!(val < -9 && wrapThresh < 1000))
	{
		sprintf(buff, "%d", val >= wrapThresh ? val%wrapThresh : val);
	}
	MN_DrTextAYellow(buff, x-MN_TextAWidth(buff)/2, y);
}
#endif // APPVER_HEXENREV

//===========================================================================
//
// DrawHubText
//
//===========================================================================

static void DrawHubText(void)
{
	int		count;
	char	*ch;
	int		c;
	int		cx, cy;
	patch_t *w;

	cy = 5;
	cx = 10;
	ch = HubText;
	count = (intertime-10)/TEXTSPEED;
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
			cx = 10;
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

#if (APPVER_HEXENREV < AV_HR_HEX11A)
//========================================================================
//
// IN_DrTextC
//
//========================================================================

static void IN_DrTextC(char *text, int x, int y)
{
	char c;
	patch_t *p;

	while((c = *text++) != 0)
	{
		if(c < 33)
		{
			y += 12;
		}
		else
		{
			p = W_CacheLumpNum(FontBLump+c-33, PU_CACHE);
			V_DrawShadowedPatch(x+(9-p->width/2), y, p);
			y += p->height+2;
		}
	}
}

#endif
