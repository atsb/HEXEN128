
//**************************************************************************
//**
//** sv_save.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: sv_save.c,v $
//** $Revision: 1.36 $
//** $Date: 95/10/17 00:10:02 $
//** $Author: bgokey $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "p_local.h"

// MACROS ------------------------------------------------------------------

#define MAX_TARGET_PLAYERS 512
#define MOBJ_NULL -1
#define MOBJ_XX_PLAYER -2
#define GET_BYTE (*SavePtr.b++)
#define GET_WORD (*SavePtr.w++)
#define GET_LONG (*SavePtr.l++)
#if (APPVER_HEXENREV < AV_HR_HEX10A)
#define MAX_MAPS 64
#define BASE_SLOT 6
#else
#define MAX_MAPS 99
#define BASE_SLOT 6
#define REBORN_SLOT 7
#define REBORN_DESCRIPTION "TEMP GAME"
#endif
#define MAX_THINKER_SIZE 256

// TYPES -------------------------------------------------------------------

typedef enum
{
	ASEG_GAME_HEADER = 101,
	ASEG_MAP_HEADER,
	ASEG_WORLD,
	ASEG_POLYOBJS,
	ASEG_MOBJS,
	ASEG_THINKERS,
	ASEG_SCRIPTS,
	ASEG_PLAYERS,
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	ASEG_SOUNDS,
#endif
	ASEG_MISC,
	ASEG_END
} gameArchiveSegment_t;

typedef enum
{
	TC_NULL,
	TC_MOVE_CEILING,
	TC_VERTICAL_DOOR,
	TC_MOVE_FLOOR,
	TC_PLAT_RAISE,
	TC_INTERPRET_ACS,
	TC_FLOOR_WAGGLE,
	TC_LIGHT,
	TC_PHASE,
	TC_BUILD_PILLAR,
	TC_ROTATE_POLY,
	TC_MOVE_POLY,
	TC_POLY_DOOR
} thinkClass_t;

typedef struct
{
	thinkClass_t tClass;
	think_t thinkerFunc;
	void (*mangleFunc)();
	void (*restoreFunc)();
	size_t size;
} thinkInfo_t;

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
} ssthinker_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void P_SpawnPlayer(mapthing_t *mthing);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void ArchiveWorld(void);
static void UnarchiveWorld(void);
static void ArchivePolyobjs(void);
static void UnarchivePolyobjs(void);
static void ArchiveMobjs(void);
static void UnarchiveMobjs(void);
static void ArchiveThinkers(void);
static void UnarchiveThinkers(void);
static void ArchiveScripts(void);
static void UnarchiveScripts(void);
static void ArchivePlayers(void);
static void UnarchivePlayers(void);
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
static void ArchiveSounds(void);
static void UnarchiveSounds(void);
#endif
static void ArchiveMisc(void);
static void UnarchiveMisc(void);
static void SetMobjArchiveNums(void);
static void RemoveAllThinkers(void);
static void MangleMobj(mobj_t *mobj);
static void RestoreMobj(mobj_t *mobj);
static int GetMobjNum(mobj_t *mobj);
static void SetMobjPtr(int *archiveNum);
static void MangleSSThinker(ssthinker_t *sst);
static void RestoreSSThinker(ssthinker_t *sst);
static void RestoreSSThinkerNoSD(ssthinker_t *sst);
static void MangleScript(acs_t *script);
static void RestoreScript(acs_t *script);
static void RestorePlatRaise(plat_t *plat);
static void RestoreMoveCeiling(ceiling_t *ceiling);
static void AssertSegment(gameArchiveSegment_t segType);
static void ClearSaveSlot(int slot);
static void CopySaveSlot(int sourceSlot, int destSlot);
static void CopyFile(char *sourceName, char *destName);
static boolean ExistingFile(char *name);
static void OpenStreamOut(char *fileName);
static void CloseStreamOut(void);
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
static void StreamOutBuffer(void *buffer, int size);
static void StreamOutByte(byte val);
static void StreamOutWord(unsigned short val);
static void StreamOutLong(unsigned int val);
#else // VERSIONS RESTORATION: A HACK. Also done between versions 1.0 and 1.2 of Heretic.
#define StreamOutBuffer(buffer, size) { memcpy(SavePtr.b, buffer, size); SavePtr.b += size; }
#define StreamOutByte(val) { *SavePtr.b++ = val; }
#define StreamOutWord(val) { *SavePtr.w++ = val; }
#define StreamOutLong(val) { *SavePtr.l++ = val; }
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int ACScriptCount;
extern byte *ActionCodeBase;
extern acsInfo_t *ACSInfo;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char *SavePath;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int MobjCount;
static mobj_t **MobjList;
static int **TargetPlayerAddrs;
static int TargetPlayerCount;
static byte *SaveBuffer;
static boolean SavingPlayers;
static union
{
	byte *b;
	short *w;
	int *l;
} SavePtr;
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
static FILE *SavingFP;
#endif

// This list has been prioritized using frequency estimates
static thinkInfo_t ThinkerInfo[] =
{
	{
		TC_MOVE_FLOOR,
		T_MoveFloor,
		MangleSSThinker,
		RestoreSSThinker,
		sizeof(floormove_t)
	},
	{
		TC_PLAT_RAISE,
		T_PlatRaise,
		MangleSSThinker,
		RestorePlatRaise,
		sizeof(plat_t)
	},
	{
		TC_MOVE_CEILING,
		T_MoveCeiling,
		MangleSSThinker,
		RestoreMoveCeiling,
		sizeof(ceiling_t)
	},
	{
		TC_LIGHT,
		T_Light,
		MangleSSThinker,
		RestoreSSThinkerNoSD,
		sizeof(light_t)
	},
	{
		TC_VERTICAL_DOOR,
		T_VerticalDoor,
		MangleSSThinker,
		RestoreSSThinker,
		sizeof(vldoor_t)
	},
	{
		TC_PHASE,
		T_Phase,
		MangleSSThinker,
		RestoreSSThinkerNoSD,
		sizeof(phase_t)
	},
	{
		TC_INTERPRET_ACS,
		T_InterpretACS,
		MangleScript,
		RestoreScript,
		sizeof(acs_t)
	},
	{
		TC_ROTATE_POLY,
		T_RotatePoly,
		NULL,
		NULL,
		sizeof(polyevent_t)
	},
	{
		TC_BUILD_PILLAR,
		T_BuildPillar,
		MangleSSThinker,
		RestoreSSThinker,
		sizeof(pillar_t)
	},
	{
		TC_MOVE_POLY,
		T_MovePoly,
		NULL,
		NULL,
		sizeof(polyevent_t)
	},
	{
		TC_POLY_DOOR,
		T_PolyDoor,
		NULL,
		NULL,
		sizeof(polydoor_t)
	},
	{
		TC_FLOOR_WAGGLE,
		T_FloorWaggle,
		MangleSSThinker,
		RestoreSSThinker,
		sizeof(floorWaggle_t)
	},
	{ // Terminator
		TC_NULL, NULL, NULL, NULL, 0
	}
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SV_SaveGame
//
//==========================================================================

void SV_SaveGame(int slot, char *description)
{
	char fileName[100];
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	char versionText[HXS_VERSION_TEXT_LENGTH];
#endif

#if (APPVER_HEXENREV < AV_HR_HEX10A)
	SaveBuffer = Z_Malloc(0x1000, PU_STATIC, NULL);
	SavePtr.b = SaveBuffer;
#else
	// Open the output file
	sprintf(fileName, "%shex6.hxs", SavePath);
	OpenStreamOut(fileName);
#endif

	// Write game save description
	StreamOutBuffer(description, HXS_DESCRIPTION_LENGTH);

	// Write version info
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	memset(SavePtr.b, 0, HXS_VERSION_TEXT_LENGTH);
	strcpy(SavePtr.b, HXS_VERSION_TEXT);
	SavePtr.b += HXS_VERSION_TEXT_LENGTH;
#else
	memset(versionText, 0, HXS_VERSION_TEXT_LENGTH);
	strcpy(versionText, HXS_VERSION_TEXT);
	StreamOutBuffer(versionText, HXS_VERSION_TEXT_LENGTH);
#endif

	// Place a header marker
	StreamOutLong(ASEG_GAME_HEADER);

	// Write current map and difficulty
	StreamOutByte(gamemap);
	StreamOutByte(gameskill);

	// Write global script info
	StreamOutBuffer(WorldVars, sizeof(WorldVars));
	StreamOutBuffer(ACSStore, sizeof(ACSStore));

	ArchivePlayers();

	// Place a termination marker
	StreamOutLong(ASEG_END);

#if (APPVER_HEXENREV < AV_HR_HEX10A)
	sprintf(fileName, "%shex6.hxs", SavePath);
	M_WriteFile(fileName, SaveBuffer, SavePtr.b - SaveBuffer);
	Z_Free(SaveBuffer);
#else
	// Close the output file
	CloseStreamOut();
#endif

	// Save out the current map
	SV_SaveMap(true); // true = save player info

	// Clear all save files at destination slot
	ClearSaveSlot(slot);

	// Copy base slot to destination slot
	CopySaveSlot(BASE_SLOT, slot);
}

//==========================================================================
//
// SV_SaveMap
//
//==========================================================================

void SV_SaveMap(boolean savePlayers)
{
	char fileName[100];
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	int len;
#endif

	SavingPlayers = savePlayers;

#if (APPVER_HEXENREV < AV_HR_HEX10A)
	SavePtr.b = SaveBuffer = Z_Malloc(0x2D000, PU_STATIC, NULL);
#else
	// Open the output file
	sprintf(fileName, "%shex6%02d.hxs", SavePath, gamemap);
	OpenStreamOut(fileName);
#endif

	// Place a header marker
	StreamOutLong(ASEG_MAP_HEADER);

	// Write the level timer
	StreamOutLong(leveltime);

	// Set the mobj archive numbers
	SetMobjArchiveNums();

	ArchiveWorld();
	ArchivePolyobjs();
	ArchiveMobjs();
	ArchiveThinkers();
	ArchiveScripts();
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	ArchiveSounds();
#endif
	ArchiveMisc();

	// Place a termination marker
	StreamOutLong(ASEG_END);

#if (APPVER_HEXENREV < AV_HR_HEX10A)
	len = SavePtr.b - SaveBuffer;
	if (len > 0x2D000)
		I_Error("SV_SaveGame: buffer overrun (%d > %d)", len, 0x2D000);
	sprintf(fileName, "%shex6%02d.hxs", SavePath, gamemap);
	M_WriteFile(fileName, SaveBuffer, len);
	Z_Free(SaveBuffer);
#else
	// Close the output file
	CloseStreamOut();
#endif
}

//==========================================================================
//
// SV_LoadGame
//
//==========================================================================

void SV_LoadGame(int slot)
{
	int i;
	char fileName[100];
	player_t playerBackup[MAXPLAYERS];
	mobj_t *mobj;

	// Copy all needed save files to the base slot
	if(slot != BASE_SLOT)
	{
		ClearSaveSlot(BASE_SLOT);
		CopySaveSlot(slot, BASE_SLOT);
	}

	// Create the name
	sprintf(fileName, "%shex6.hxs", SavePath);

	// Load the file
	M_ReadFile(fileName, &SaveBuffer);

	// Set the save pointer and skip the description field
	SavePtr.b = SaveBuffer+HXS_DESCRIPTION_LENGTH;

	// Check the version text
	if(strcmp(SavePtr.b, HXS_VERSION_TEXT))
	{ // Bad version
		return;
	}
	SavePtr.b += HXS_VERSION_TEXT_LENGTH;

	AssertSegment(ASEG_GAME_HEADER);

	gameepisode = 1;
	gamemap = GET_BYTE;
	gameskill = GET_BYTE;

	// Read global script info
	memcpy(WorldVars, SavePtr.b, sizeof(WorldVars));
	SavePtr.b += sizeof(WorldVars);
	memcpy(ACSStore, SavePtr.b, sizeof(ACSStore));
	SavePtr.b += sizeof(ACSStore);

	// Read the player structures
	UnarchivePlayers();

	AssertSegment(ASEG_END);

	Z_Free(SaveBuffer);

	// Save player structs
	for(i = 0; i < MAXPLAYERS; i++)
	{
		playerBackup[i] = players[i];
	}

	// Load the current map
	SV_LoadMap();

	// Don't need the player mobj relocation info for load game
	Z_Free(TargetPlayerAddrs);

	// Restore player structs
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	inv_ptr = 0;
	curpos = 0;
#endif
	for(i = 0; i < MAXPLAYERS; i++)
	{
		mobj = players[i].mo;
		players[i] = playerBackup[i];
		players[i].mo = mobj;
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
		if(i == consoleplayer)
		{
			players[i].readyArtifact = players[i].inventory[inv_ptr].type;
		}
#endif
	}
}

#if (APPVER_HEXENREV >= AV_HR_HEX10A)
//==========================================================================
//
// SV_UpdateRebornSlot
//
// Copies the base slot to the reborn slot.
//
//==========================================================================

void SV_UpdateRebornSlot(void)
{
	ClearSaveSlot(REBORN_SLOT);
	CopySaveSlot(BASE_SLOT, REBORN_SLOT);
}

//==========================================================================
//
// SV_ClearRebornSlot
//
//==========================================================================

void SV_ClearRebornSlot(void)
{
	ClearSaveSlot(REBORN_SLOT);
}

//==========================================================================
//
// SV_MapTeleport
//
//==========================================================================
#endif // APPVER_HEXENREV >= AV_HR_HEX10A

void SV_MapTeleport(int map, int position)
{
	int i;
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOB)
	int j;
#endif
	char fileName[100];
	player_t playerBackup[MAXPLAYERS];
	mobj_t *targetPlayerMobj;
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	mobj_t *mobj;
#endif
	int inventoryPtr;
	int currentInvPos;
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	boolean rClass;
#endif
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOB)
	boolean playerWasReborn;
	boolean oldWeaponowned[NUMWEAPONS];
	int oldKeys;
	int oldPieces;
	int bestWeapon;
#endif

	if(!deathmatch)
	{
		if(P_GetMapCluster(gamemap) == P_GetMapCluster(map))
		{ // Same cluster - save map without saving player mobjs
			SV_SaveMap(false);
		}
		else
		{ // Entering new cluster - clear base slot
			ClearSaveSlot(BASE_SLOT);
		}
	}

	// Store player structs for later
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	rClass = randomclass;
	randomclass = false;
#endif
	for(i = 0; i < MAXPLAYERS; i++)
	{
		playerBackup[i] = players[i];
	}

	// Save some globals that get trashed during the load
	inventoryPtr = inv_ptr;
	currentInvPos = curpos;

	// Only SV_LoadMap() uses TargetPlayerAddrs, so it's NULLed here
	// for the following check (player mobj redirection)
	TargetPlayerAddrs = NULL;

	gamemap = map;
	sprintf(fileName, "%shex6%02d.hxs", SavePath, gamemap);
	if(!deathmatch && ExistingFile(fileName))
	{ // Unarchive map
		SV_LoadMap();
	}
	else
	{ // New map
		G_InitNew(gameskill, gameepisode, gamemap);

		// Destroy all freshly spawned players
		for(i = 0; i < MAXPLAYERS; i++)
		{
			if(playeringame[i])
			{
				P_RemoveMobj(players[i].mo);
			}
		}
	}

	// Restore player structs
	targetPlayerMobj = NULL;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!playeringame[i])
		{
			continue;
		}
		players[i] = playerBackup[i];
#if (APPVER_HEXENREV < AV_HR_HEX10A) // From Heretic
		players[i].message = NULL;
#else
		P_ClearMessage(&players[i]);
#endif
		players[i].attacker = NULL;
		players[i].poisoner = NULL;

#if (APPVER_HEXENREV < AV_HR_HEX10A)
		if(deathmatch)
#else
		if(netgame)
#endif
		{
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA)
			players[i].playerstate = PST_REBORN;
			G_DeathMatchSpawnPlayer(i);
#else
			if(players[i].playerstate == PST_DEAD)
			{ // In a network game, force all players to be alive
				players[i].playerstate = PST_REBORN;
			}
#endif
	#if (APPVER_HEXENREV >= AV_HR_HEXDEMOB)
			if(!deathmatch)
			{ // Cooperative net-play, retain keys and weapons
				oldKeys = players[i].keys;
				oldPieces = players[i].pieces;
				for(j = 0; j < NUMWEAPONS; j++)
				{
					oldWeaponowned[j] = players[i].weaponowned[j];
				}
			}
	#endif
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
		}
	#if (APPVER_HEXENREV >= AV_HR_HEXDEMOB)
		playerWasReborn = (players[i].playerstate == PST_REBORN);
	#endif
		if(deathmatch)
		{
#endif
	#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
			memset(players[i].frags, 0, sizeof(players[i].frags));
	#if (APPVER_HEXENREV >= AV_HR_HEX10A)
			mobj = P_SpawnMobj(playerstarts[0][i].x<<16,
				playerstarts[0][i].y<<16, 0, MT_PLAYER_FIGHTER);
			players[i].mo = mobj;
	#endif
			G_DeathMatchSpawnPlayer(i);
	#if (APPVER_HEXENREV >= AV_HR_HEX10A)
			P_RemoveMobj(mobj);
	#endif
	#endif // APPVER_HEXENREV >= AV_HR_HEXDEMOA
		}
		else
		{
			P_SpawnPlayer(&playerstarts[position][i]);
		}

#if (APPVER_HEXENREV >= AV_HR_HEXDEMOB)
		if(playerWasReborn && netgame && !deathmatch)
		{ // Restore keys and weapons when reborn in co-op
			players[i].keys = oldKeys;
			players[i].pieces = oldPieces;
			for(bestWeapon = 0, j = 0; j < NUMWEAPONS; j++)
			{
				if(oldWeaponowned[j])
				{
					bestWeapon = j;
					players[i].weaponowned[j] = true;
				}
			}
			players[i].mana[MANA_1] = 25;
			players[i].mana[MANA_2] = 25;
			if(bestWeapon)
			{ // Bring up the best weapon
				players[i].pendingweapon = bestWeapon;
			}
		}
#endif

		if(targetPlayerMobj == NULL)
		{ // The poor sap
			targetPlayerMobj = players[i].mo;
		}
	}
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	randomclass = rClass;
#endif

	// Redirect anything targeting a player mobj
	if(TargetPlayerAddrs)
	{
		for(i = 0; i < TargetPlayerCount; i++)
		{
			*TargetPlayerAddrs[i] = (int)targetPlayerMobj;
		}
		Z_Free(TargetPlayerAddrs);
	}

#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	// Destroy all things touching players
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(playeringame[i])
		{
			P_TeleportMove(players[i].mo, players[i].mo->x,
				players[i].mo->y);
		}
	}
#endif

	// Restore trashed globals
	inv_ptr = inventoryPtr;
	curpos = currentInvPos;

	// Launch waiting scripts
	if(!deathmatch)
	{
		P_CheckACSStore();
	}

#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	// For single play, save immediately into the reborn slot
	if(!netgame)
	{
		SV_SaveGame(REBORN_SLOT, REBORN_DESCRIPTION);
	}
#endif
}

#if (APPVER_HEXENREV >= AV_HR_HEX10A)
//==========================================================================
//
// SV_GetRebornSlot
//
//==========================================================================

int SV_GetRebornSlot(void)
{
	return(REBORN_SLOT);
}

//==========================================================================
//
// SV_RebornSlotAvailable
//
// Returns true if the reborn slot is available.
//
//==========================================================================

boolean SV_RebornSlotAvailable(void)
{
	char fileName[100];

	sprintf(fileName, "%shex%d.hxs", SavePath, REBORN_SLOT);
	return ExistingFile(fileName);
}
#endif // APPVER_HEXENREV >= AV_HR_HEX10A

//==========================================================================
//
// SV_LoadMap
//
//==========================================================================

void SV_LoadMap(void)
{
	char fileName[100];

	// Load a base level
	G_InitNew(gameskill, gameepisode, gamemap);

	// Remove all thinkers
	RemoveAllThinkers();

	// Create the name
	sprintf(fileName, "%shex6%02d.hxs", SavePath, gamemap);

	// Load the file
	M_ReadFile(fileName, &SaveBuffer);
	SavePtr.b = SaveBuffer;

	AssertSegment(ASEG_MAP_HEADER);

	// Read the level timer
	leveltime = GET_LONG;

	UnarchiveWorld();
	UnarchivePolyobjs();
	UnarchiveMobjs();
	UnarchiveThinkers();
	UnarchiveScripts();
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	UnarchiveSounds();
#endif
	UnarchiveMisc();

	AssertSegment(ASEG_END);

	// Free mobj list and save buffer
	Z_Free(MobjList);
	Z_Free(SaveBuffer);
}

//==========================================================================
//
// SV_InitBaseSlot
//
//==========================================================================

void SV_InitBaseSlot(void)
{
	ClearSaveSlot(BASE_SLOT);
}

//==========================================================================
//
// ArchivePlayers
//
//==========================================================================

static void ArchivePlayers(void)
{
	int i;
	int j;
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	player_t *tempPlayer;
#else
	player_t tempPlayer;
#endif

	StreamOutLong(ASEG_PLAYERS);
	for(i = 0; i < MAXPLAYERS; i++)
	{
		StreamOutByte(playeringame[i]);
	}
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!playeringame[i])
		{
			continue;
		}
#if (APPVER_HEXENREV < AV_HR_HEX10A)
		StreamOutByte(PlayerClass[i]);
		tempPlayer = (player_t *)SavePtr.b;
		memcpy(tempPlayer, &players[i], sizeof(player_t));
		SavePtr.b += sizeof(player_t);
		for(j = 0; j < NUMPSPRITES; j++)
		{
			if(tempPlayer->psprites[j].state)
			{
				tempPlayer->psprites[j].state =
					(state_t *)(tempPlayer->psprites[j].state-states);
			}
		}
#else
		StreamOutByte(PlayerClass[i]);
		tempPlayer = players[i];
		for(j = 0; j < NUMPSPRITES; j++)
		{
			if(tempPlayer.psprites[j].state)
			{
				tempPlayer.psprites[j].state =
					(state_t *)(tempPlayer.psprites[j].state-states);
			}
		}
		StreamOutBuffer(&tempPlayer, sizeof(player_t));
#endif
	}
}

//==========================================================================
//
// UnarchivePlayers
//
//==========================================================================

static void UnarchivePlayers(void)
{
	int i, j;

	AssertSegment(ASEG_PLAYERS);
	for(i = 0; i < MAXPLAYERS; i++)
	{
		playeringame[i] = GET_BYTE;
	}
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!playeringame[i])
		{
			continue;
		}
		PlayerClass[i] = GET_BYTE;
		memcpy(&players[i], SavePtr.b, sizeof(player_t));
		SavePtr.b += sizeof(player_t);
		players[i].mo = NULL; // Will be set when unarc thinker
#if (APPVER_HEXENREV < AV_HR_HEX10A) // From Heretic
		players[i].message = NULL;
#else
		P_ClearMessage(&players[i]);
#endif
		players[i].attacker = NULL;
		players[i].poisoner = NULL;
		for(j = 0; j < NUMPSPRITES; j++)
		{
			if(players[i].psprites[j].state)
			{
				players[i].psprites[j].state =
					&states[(int)players[i].psprites[j].state];
			}
		}
	}
}

//==========================================================================
//
// ArchiveWorld
//
//==========================================================================

static void ArchiveWorld(void)
{
	int i;
	int j;
	sector_t *sec;
	line_t *li;
	side_t *si;

	StreamOutLong(ASEG_WORLD);
	for(i = 0, sec = sectors; i < numsectors; i++, sec++)
	{
		StreamOutWord(sec->floorheight>>FRACBITS);
		StreamOutWord(sec->ceilingheight>>FRACBITS);
		StreamOutWord(sec->floorpic);
		StreamOutWord(sec->ceilingpic);
		StreamOutWord(sec->lightlevel);
		StreamOutWord(sec->special);
		StreamOutWord(sec->tag);
		StreamOutWord(sec->seqType);
	}
	for(i = 0, li = lines; i < numlines; i++, li++)
	{
		StreamOutWord(li->flags);
		StreamOutByte(li->special);
		StreamOutByte(li->arg1);
		StreamOutByte(li->arg2);
		StreamOutByte(li->arg3);
		StreamOutByte(li->arg4);
		StreamOutByte(li->arg5);
		for(j = 0; j < 2; j++)
		{
			if(li->sidenum[j] == -1)
			{
				continue;
			}
			si = &sides[li->sidenum[j]];
			StreamOutWord(si->textureoffset>>FRACBITS);
			StreamOutWord(si->rowoffset>>FRACBITS);
			StreamOutWord(si->toptexture);
			StreamOutWord(si->bottomtexture);
			StreamOutWord(si->midtexture);
		}
	}
}

//==========================================================================
//
// UnarchiveWorld
//
//==========================================================================

static void UnarchiveWorld(void)
{
	int i;
	int j;
	sector_t *sec;
	line_t *li;
	side_t *si;

	AssertSegment(ASEG_WORLD);
	for(i = 0, sec = sectors; i < numsectors; i++, sec++)
	{
		sec->floorheight = GET_WORD<<FRACBITS;
		sec->ceilingheight = GET_WORD<<FRACBITS;
		sec->floorpic = GET_WORD;
		sec->ceilingpic = GET_WORD;
		sec->lightlevel = GET_WORD;
		sec->special = GET_WORD;
		sec->tag = GET_WORD;
		sec->seqType = GET_WORD;
		sec->specialdata = 0;
		sec->soundtarget = 0;
	}
	for(i = 0, li = lines; i < numlines; i++, li++)
	{
		li->flags = GET_WORD;
		li->special = GET_BYTE;
		li->arg1 = GET_BYTE;
		li->arg2 = GET_BYTE;
		li->arg3 = GET_BYTE;
		li->arg4 = GET_BYTE;
		li->arg5 = GET_BYTE;
		for(j = 0; j < 2; j++)
		{
			if(li->sidenum[j] == -1)
			{
				continue;
			}
			si = &sides[li->sidenum[j]];
			si->textureoffset = GET_WORD<<FRACBITS;
			si->rowoffset = GET_WORD<<FRACBITS;
			si->toptexture = GET_WORD;
			si->bottomtexture = GET_WORD;
			si->midtexture = GET_WORD;
		}
	}
}

//==========================================================================
//
// SetMobjArchiveNums
//
// Sets the archive numbers in all mobj structs.  Also sets the MobjCount
// global.  Ignores player mobjs if SavingPlayers is false.
//
//==========================================================================

static void SetMobjArchiveNums(void)
{
	mobj_t *mobj;
	thinker_t *thinker;

	MobjCount = 0;
	for(thinker = thinkercap.next; thinker != &thinkercap;
		thinker = thinker->next)
	{
		if(thinker->function == P_MobjThinker)
		{
			mobj = (mobj_t *)thinker;
			if(mobj->player && !SavingPlayers)
			{ // Skipping player mobjs
				continue;
			}
			mobj->archiveNum = MobjCount++;
		}
	}
}

//==========================================================================
//
// ArchiveMobjs
//
//==========================================================================

static void ArchiveMobjs(void)
{
	int count;
	thinker_t *thinker;
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	mobj_t tempMobj;
#endif

	StreamOutLong(ASEG_MOBJS);
	StreamOutLong(MobjCount);
	count = 0;
	for(thinker = thinkercap.next; thinker != &thinkercap;
		thinker = thinker->next)
	{
		if(thinker->function != P_MobjThinker)
		{ // Not a mobj thinker
			continue;
		}
		if(((mobj_t *)thinker)->player && !SavingPlayers)
		{ // Skipping player mobjs
			continue;
		}
#if (APPVER_HEXENREV < AV_HR_HEX10A)
		memcpy(SavePtr.b, thinker, sizeof(mobj_t));
		MangleMobj((mobj_t *)SavePtr.b);
		count++;
		SavePtr.b += sizeof(mobj_t);
#else
		count++;
		memcpy(&tempMobj, thinker, sizeof(mobj_t));
		MangleMobj(&tempMobj);
		StreamOutBuffer(&tempMobj, sizeof(mobj_t));
#endif
	}
	if(count != MobjCount)
	{
		I_Error("ArchiveMobjs: bad mobj count");
	}
}

//==========================================================================
//
// UnarchiveMobjs
//
//==========================================================================

static void UnarchiveMobjs(void)
{
	int i;
	mobj_t *mobj;

	AssertSegment(ASEG_MOBJS);
	TargetPlayerAddrs = Z_Malloc(MAX_TARGET_PLAYERS*sizeof(int *),
		PU_STATIC, NULL);
	TargetPlayerCount = 0;
	MobjCount = GET_LONG;
	MobjList = Z_Malloc(MobjCount*sizeof(mobj_t *), PU_STATIC, NULL);
	for(i = 0; i < MobjCount; i++)
	{
		MobjList[i] = Z_Malloc(sizeof(mobj_t), PU_LEVEL, NULL);
	}
	for(i = 0; i < MobjCount; i++)
	{
		mobj = MobjList[i];
		memcpy(mobj, SavePtr.b, sizeof(mobj_t));
		SavePtr.b += sizeof(mobj_t);
		mobj->thinker.function = P_MobjThinker;
		RestoreMobj(mobj);
		P_AddThinker(&mobj->thinker);
	}
	P_CreateTIDList();
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	P_InitCreatureCorpseQueue(true); // true = scan for corpses
#endif
}

//==========================================================================
//
// MangleMobj
//
//==========================================================================

static void MangleMobj(mobj_t *mobj)
{
	boolean corpse;

	corpse = mobj->flags&MF_CORPSE;
	mobj->state = (state_t *)(mobj->state-states);
	if(mobj->player)
	{
		mobj->player = (player_t *)((mobj->player-players)+1);
	}
	if(corpse)
	{
		mobj->target = (mobj_t *)MOBJ_NULL;
	}
	else
	{
		mobj->target = (mobj_t *)GetMobjNum(mobj->target);
	}
	switch(mobj->type)
	{
		// Just special1
#ifndef APPVER_DEMO
		case MT_BISH_FX:
#endif
		case MT_HOLY_FX:
#ifndef APPVER_DEMO
		case MT_DRAGON:
#endif
		case MT_THRUSTFLOOR_UP:
		case MT_THRUSTFLOOR_DOWN:
		case MT_MINOTAUR:
#ifndef APPVER_DEMO
		case MT_SORCFX1:
#endif
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
		case MT_MSTAFF_FX2:
#endif
			if(corpse)
			{
				mobj->special1 = MOBJ_NULL;
			}
			else
			{
				mobj->special1 = GetMobjNum((mobj_t *)mobj->special1);
			}
			break;

		// Just special2
		case MT_LIGHTNING_FLOOR:
		case MT_LIGHTNING_ZAP:
			if(corpse)
			{
				mobj->special2 = MOBJ_NULL;
			}
			else
			{
				mobj->special2 = GetMobjNum((mobj_t *)mobj->special2);
			}
			break;

		// Both special1 and special2
		case MT_HOLY_TAIL:
		case MT_LIGHTNING_CEILING:
			if(corpse)
			{
				mobj->special1 = MOBJ_NULL;
				mobj->special2 = MOBJ_NULL;
			}
			else
			{
				mobj->special1 = GetMobjNum((mobj_t *)mobj->special1);
				mobj->special2 = GetMobjNum((mobj_t *)mobj->special2);
			}
			break;

#ifndef APPVER_DEMO
		// Miscellaneous
		case MT_KORAX:
			mobj->special1 = 0; // Searching index
			break;
#endif

		default:
			break;
	}
}

//==========================================================================
//
// GetMobjNum
//
//==========================================================================

static int GetMobjNum(mobj_t *mobj)
{
	if(mobj == NULL)
	{
		return MOBJ_NULL;
	}
	if(mobj->player && !SavingPlayers)
	{
		return MOBJ_XX_PLAYER;
	}
	return mobj->archiveNum;
}

//==========================================================================
//
// RestoreMobj
//
//==========================================================================

static void RestoreMobj(mobj_t *mobj)
{
	mobj->state = &states[(int)mobj->state];
	if(mobj->player)
	{
		mobj->player = &players[(int)mobj->player-1];
		mobj->player->mo = mobj;
	}
	P_SetThingPosition(mobj);
	mobj->info = &mobjinfo[mobj->type];
	mobj->floorz = mobj->subsector->sector->floorheight;
	mobj->ceilingz = mobj->subsector->sector->ceilingheight;
	SetMobjPtr((int *)&mobj->target);
	switch(mobj->type)
	{
		// Just special1
#ifndef APPVER_DEMO
		case MT_BISH_FX:
#endif
		case MT_HOLY_FX:
#ifndef APPVER_DEMO
		case MT_DRAGON:
#endif
		case MT_THRUSTFLOOR_UP:
		case MT_THRUSTFLOOR_DOWN:
		case MT_MINOTAUR:
#ifndef APPVER_DEMO
		case MT_SORCFX1:
#endif
			SetMobjPtr(&mobj->special1);
			break;

		// Just special2
		case MT_LIGHTNING_FLOOR:
		case MT_LIGHTNING_ZAP:
			SetMobjPtr(&mobj->special2);
			break;

		// Both special1 and special2
		case MT_HOLY_TAIL:
		case MT_LIGHTNING_CEILING:
			SetMobjPtr(&mobj->special1);
			SetMobjPtr(&mobj->special2);
			break;

		default:
			break;
	}
}

//==========================================================================
//
// SetMobjPtr
//
//==========================================================================

static void SetMobjPtr(int *archiveNum)
{
	if(*archiveNum == MOBJ_NULL)
	{
		*archiveNum = 0;
		return;
	}
	if(*archiveNum == MOBJ_XX_PLAYER)
	{
		if(TargetPlayerCount == MAX_TARGET_PLAYERS)
		{
			I_Error("RestoreMobj: exceeded MAX_TARGET_PLAYERS");
		}
		TargetPlayerAddrs[TargetPlayerCount++] = archiveNum;
		*archiveNum = 0;
		return;
	}
	*archiveNum = (int)MobjList[*archiveNum];
}

//==========================================================================
//
// ArchiveThinkers
//
//==========================================================================

static void ArchiveThinkers(void)
{
	thinker_t *thinker;
	thinkInfo_t *info;
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	byte buffer[MAX_THINKER_SIZE];
#endif

	StreamOutLong(ASEG_THINKERS);
	for(thinker = thinkercap.next; thinker != &thinkercap;
		thinker = thinker->next)
	{
		for(info = ThinkerInfo; info->tClass != TC_NULL; info++)
		{
			if(thinker->function == info->thinkerFunc)
			{
				StreamOutByte(info->tClass);
#if (APPVER_HEXENREV < AV_HR_HEX10A)
				memcpy(SavePtr.b, thinker, info->size);
				if(info->mangleFunc)
				{
					info->mangleFunc(SavePtr.b);
				}
				SavePtr.b += info->size;
#else
				memcpy(buffer, thinker, info->size);
				if(info->mangleFunc)
				{
					info->mangleFunc(buffer);
				}
				StreamOutBuffer(buffer, info->size);
#endif
				break;
			}
		}
	}
	// Add a termination marker
	StreamOutByte(TC_NULL);
}

//==========================================================================
//
// UnarchiveThinkers
//
//==========================================================================

static void UnarchiveThinkers(void)
{
	int tClass;
	thinker_t *thinker;
	thinkInfo_t *info;

	AssertSegment(ASEG_THINKERS);
	while((tClass = GET_BYTE) != TC_NULL)
	{
		for(info = ThinkerInfo; info->tClass != TC_NULL; info++)
		{
			if(tClass == info->tClass)
			{
				thinker = Z_Malloc(info->size, PU_LEVEL, NULL);
				memcpy(thinker, SavePtr.b, info->size);
				SavePtr.b += info->size;
				thinker->function = info->thinkerFunc;
				if(info->restoreFunc)
				{
					info->restoreFunc(thinker);
				}
				P_AddThinker(thinker);
				break;
			}
		}
		if(info->tClass == TC_NULL)
		{
			I_Error("UnarchiveThinkers: Unknown tClass %d in "
				"savegame", tClass);
		}
	}
}

//==========================================================================
//
// MangleSSThinker
//
//==========================================================================

static void MangleSSThinker(ssthinker_t *sst)
{
	sst->sector = (sector_t *)(sst->sector-sectors);
}

//==========================================================================
//
// RestoreSSThinker
//
//==========================================================================

static void RestoreSSThinker(ssthinker_t *sst)
{
	sst->sector = &sectors[(int)sst->sector];
	sst->sector->specialdata = sst->thinker.function;
}

//==========================================================================
//
// RestoreSSThinkerNoSD
//
//==========================================================================

static void RestoreSSThinkerNoSD(ssthinker_t *sst)
{
	sst->sector = &sectors[(int)sst->sector];
}

//==========================================================================
//
// MangleScript
//
//==========================================================================

static void MangleScript(acs_t *script)
{
	script->ip = (int *)((int)(script->ip)-(int)ActionCodeBase);
	script->line = script->line ?
		(line_t *)(script->line-lines) : (line_t *)-1;
	script->activator = (mobj_t *)GetMobjNum(script->activator);
}

//==========================================================================
//
// RestoreScript
//
//==========================================================================

static void RestoreScript(acs_t *script)
{
	script->ip = (int *)(ActionCodeBase+(int)script->ip);
	if((int)script->line == -1)
	{
		script->line = NULL;
	}
	else
	{
		script->line = &lines[(int)script->line];
	}
	SetMobjPtr((int *)&script->activator);
}

//==========================================================================
//
// RestorePlatRaise
//
//==========================================================================

static void RestorePlatRaise(plat_t *plat)
{
	plat->sector = &sectors[(int)plat->sector];
	plat->sector->specialdata = T_PlatRaise;
	P_AddActivePlat(plat);
}

//==========================================================================
//
// RestoreMoveCeiling
//
//==========================================================================

static void RestoreMoveCeiling(ceiling_t *ceiling)
{
	ceiling->sector = &sectors[(int)ceiling->sector];
	ceiling->sector->specialdata = T_MoveCeiling;
	P_AddActiveCeiling(ceiling);
}

//==========================================================================
//
// ArchiveScripts
//
//==========================================================================

static void ArchiveScripts(void)
{
	int i;

	StreamOutLong(ASEG_SCRIPTS);
	for(i = 0; i < ACScriptCount; i++)
	{
		StreamOutWord(ACSInfo[i].state);
		StreamOutWord(ACSInfo[i].waitValue);
	}
	StreamOutBuffer(MapVars, sizeof(MapVars));
}

//==========================================================================
//
// UnarchiveScripts
//
//==========================================================================

static void UnarchiveScripts(void)
{
	int i;

	AssertSegment(ASEG_SCRIPTS);
	for(i = 0; i < ACScriptCount; i++)
	{
		ACSInfo[i].state = GET_WORD;
		ACSInfo[i].waitValue = GET_WORD;
	}
	memcpy(MapVars, SavePtr.b, sizeof(MapVars));
	SavePtr.b += sizeof(MapVars);
}

//==========================================================================
//
// ArchiveMisc
//
//==========================================================================

static void ArchiveMisc(void)
{
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	int ix;
#endif

	StreamOutLong(ASEG_MISC);
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	StreamOutLong(localQuakeHappening);
#else
	for (ix=0; ix<MAXPLAYERS; ix++)
	{
		StreamOutLong(localQuakeHappening[ix]);
	}
#endif
}

//==========================================================================
//
// UnarchiveMisc
//
//==========================================================================

static void UnarchiveMisc(void)
{
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
	int ix;
#endif

	AssertSegment(ASEG_MISC);
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	localQuakeHappening = GET_LONG;
#else
	for (ix=0; ix<MAXPLAYERS; ix++)
	{
		localQuakeHappening[ix] = GET_LONG;
	}
#endif
}

//==========================================================================
//
// RemoveAllThinkers
//
//==========================================================================

static void RemoveAllThinkers(void)
{
	thinker_t *thinker;
	thinker_t *nextThinker;

	thinker = thinkercap.next;
	while(thinker != &thinkercap)
	{
		nextThinker = thinker->next;
		if(thinker->function == P_MobjThinker)
		{
			P_RemoveMobj((mobj_t *)thinker);
		}
		else
		{
			Z_Free(thinker);
		}
		thinker = nextThinker;
	}
	P_InitThinkers();
}

#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
//==========================================================================
//
// ArchiveSounds
//
//==========================================================================

static void ArchiveSounds(void)
{
	seqnode_t *node;
	sector_t *sec;
	int difference;
	int i;

	StreamOutLong(ASEG_SOUNDS);

	// Save the sound sequences
	StreamOutLong(ActiveSequences);
	for(node = SequenceListHead; node; node = node->next)
	{
		StreamOutLong(node->sequence);
		StreamOutLong(node->delayTics);
		StreamOutLong(node->volume);
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
		StreamOutLong(SN_GetSequenceOffset(node->sequence,
			node->sequencePtr));
#endif
		StreamOutLong(node->currentSoundID);
		for(i = 0; i < po_NumPolyobjs; i++)
		{
			if(node->mobj == (mobj_t *)&polyobjs[i].startSpot)
			{
				break;
			}
		}
		if(i == po_NumPolyobjs)
		{ // Sound is attached to a sector, not a polyobj
			sec = R_PointInSubsector(node->mobj->x, node->mobj->y)->sector;
			difference = (int)((byte *)sec
				-(byte *)&sectors[0])/sizeof(sector_t);
			StreamOutLong(0); // 0 -- sector sound origin
		}
		else
		{
			StreamOutLong(1); // 1 -- polyobj sound origin
			difference = i;
		}
		StreamOutLong(difference);
	}
}

//==========================================================================
//
// UnarchiveSounds
//
//==========================================================================

static void UnarchiveSounds(void)
{
	int i;
	int numSequences;
	int sequence;
	int delayTics;
	int volume;
	int seqOffset;
	int soundID;
	int polySnd;
	int secNum;
	mobj_t *sndMobj;

	AssertSegment(ASEG_SOUNDS);

	// Reload and restart all sound sequences
	numSequences = GET_LONG;
	i = 0;
	while(i < numSequences)
	{
		sequence = GET_LONG;
		delayTics = GET_LONG;
		volume = GET_LONG;
		seqOffset = GET_LONG;

		soundID = GET_LONG;
		polySnd = GET_LONG;
		secNum = GET_LONG;
		if(!polySnd)
		{
			sndMobj = (mobj_t *)&sectors[secNum].soundorg;
		}
		else
		{
			sndMobj = (mobj_t *)&polyobjs[secNum].startSpot;
		}
		SN_StartSequence(sndMobj, sequence);
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
		SN_ChangeNodeData(i, seqOffset, delayTics, volume, soundID);
#endif
		i++;
	}
}
#endif // APPVER_HEXENREV >= AV_HR_HEXDEMOA

//==========================================================================
//
// ArchivePolyobjs
//
//==========================================================================

static void ArchivePolyobjs(void)
{
	int i;

	StreamOutLong(ASEG_POLYOBJS);
	StreamOutLong(po_NumPolyobjs);
	for(i = 0; i < po_NumPolyobjs; i++)
	{
		StreamOutLong(polyobjs[i].tag);
		StreamOutLong(polyobjs[i].angle);
		StreamOutLong(polyobjs[i].startSpot.x);
		StreamOutLong(polyobjs[i].startSpot.y);
  	}
}

//==========================================================================
//
// UnarchivePolyobjs
//
//==========================================================================

static void UnarchivePolyobjs(void)
{
	int i;
	fixed_t deltaX;
	fixed_t deltaY;

	AssertSegment(ASEG_POLYOBJS);
	if(GET_LONG != po_NumPolyobjs)
	{
		I_Error("UnarchivePolyobjs: Bad polyobj count");
	}
	for(i = 0; i < po_NumPolyobjs; i++)
	{
		if(GET_LONG != polyobjs[i].tag)
		{
			I_Error("UnarchivePolyobjs: Invalid polyobj tag");
		}
		PO_RotatePolyobj(polyobjs[i].tag, (angle_t)GET_LONG);
		deltaX = GET_LONG-polyobjs[i].startSpot.x;
		deltaY = GET_LONG-polyobjs[i].startSpot.y;
		PO_MovePolyobj(polyobjs[i].tag, deltaX, deltaY);
	}
}

//==========================================================================
//
// AssertSegment
//
//==========================================================================

static void AssertSegment(gameArchiveSegment_t segType)
{
	if(GET_LONG != segType)
	{
		I_Error("Corrupt save game: Segment [%d] failed alignment check",
			segType);
	}
}

//==========================================================================
//
// ClearSaveSlot
//
// Deletes all save game files associated with a slot number.
//
//==========================================================================

static void ClearSaveSlot(int slot)
{
	int i;
	char fileName[100];

	for(i = 0; i < MAX_MAPS; i++)
	{
		sprintf(fileName, "%shex%d%02d.hxs", SavePath, slot, i);
		remove(fileName);
	}
	sprintf(fileName, "%shex%d.hxs", SavePath, slot);
	remove(fileName);
}

//==========================================================================
//
// CopySaveSlot
//
// Copies all the save game files from one slot to another.
//
//==========================================================================

static void CopySaveSlot(int sourceSlot, int destSlot)
{
	int i;
	char sourceName[100];
	char destName[100];

	for(i = 0; i < MAX_MAPS; i++)
	{
		sprintf(sourceName, "%shex%d%02d.hxs", SavePath, sourceSlot, i);
		if(ExistingFile(sourceName))
		{
			sprintf(destName, "%shex%d%02d.hxs", SavePath, destSlot, i);
			CopyFile(sourceName, destName);
		}
	}
	sprintf(sourceName, "%shex%d.hxs", SavePath, sourceSlot);
	if(ExistingFile(sourceName))
	{
		sprintf(destName, "%shex%d.hxs", SavePath, destSlot);
		CopyFile(sourceName, destName);
	}
}

//==========================================================================
//
// CopyFile
//
//==========================================================================

static void CopyFile(char *sourceName, char *destName)
{
	int length;
	byte *buffer;

	length = M_ReadFile(sourceName, &buffer);
	M_WriteFile(destName, buffer, length);
	Z_Free(buffer);
}

//==========================================================================
//
// ExistingFile
//
//==========================================================================

static boolean ExistingFile(char *name)
{
	FILE *fp;

	if((fp = fopen(name, "rb")) != NULL)
	{
		fclose(fp);
		return true;
	}
	else
	{
		return false;
	}
}

#if (APPVER_HEXENREV >= AV_HR_HEX10A)
//==========================================================================
//
// OpenStreamOut
//
//==========================================================================

static void OpenStreamOut(char *fileName)
{
	SavingFP = fopen(fileName, "wb");
}

//==========================================================================
//
// CloseStreamOut
//
//==========================================================================

static void CloseStreamOut(void)
{
	if(SavingFP)
	{
		fclose(SavingFP);
	}
}

//==========================================================================
//
// StreamOutBuffer
//
//==========================================================================

static void StreamOutBuffer(void *buffer, int size)
{
	fwrite(buffer, size, 1, SavingFP);
}

//==========================================================================
//
// StreamOutByte
//
//==========================================================================

static void StreamOutByte(byte val)
{
	fwrite(&val, sizeof(byte), 1, SavingFP);
}

//==========================================================================
//
// StreamOutWord
//
//==========================================================================

static void StreamOutWord(unsigned short val)
{
	fwrite(&val, sizeof(unsigned short), 1, SavingFP);
}

//==========================================================================
//
// StreamOutLong
//
//==========================================================================

static void StreamOutLong(unsigned int val)
{
	fwrite(&val, sizeof(int), 1, SavingFP);
}
#endif // APPVER_HEXENREV >= AV_HR_HEX10A
