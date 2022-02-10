
// I_SOUND.C

#include "gamever.h" // VERSIONS RESTORATION: We need to include this early
#if (APPVER_HEXENREV < AV_HR_HEX10A)
#include <dos.h>
#include <conio.h>
#include <stdarg.h>
#include <graph.h>
#include "h2def.h"
#include "r_local.h"
#include "p_local.h"    // for P_AproxDistance
#include "sounds.h"
#include "i_sound.h"
#include "soundst.h"
#include "st_start.h"
#include "dmx.h"
#include "dpmiapi.h"
#else
#include <stdio.h>
#include "h2def.h"
#include "dmx.h"
#include "sounds.h"
#include "i_sound.h"
#endif

/*
===============
=
= I_StartupTimer
=
===============
*/

int tsm_ID = -1;

void I_StartupTimer (void)
{
#ifndef NOTIMER
	extern int I_TimerISR(void);

#if (APPVER_HEXENREV < AV_HR_HEX10A)
	ST_Message("I_StartupTimer()\n");
#else
	ST_Message("  I_StartupTimer()\n");
#endif
	// installs master timer.  Must be done before StartupTimer()!
	TSM_Install(SND_TICRATE);
	tsm_ID = TSM_NewService (I_TimerISR, 35, 255, 0); // max priority
	if (tsm_ID == -1)
	{
		I_Error("Can't register 35 Hz timer w/ DMX library");
	}
#endif
}

void I_ShutdownTimer (void)
{
	TSM_DelService(tsm_ID);
	TSM_Remove();
}

/*
 *
 *                           SOUND HEADER & DATA
 *
 *
 */

#if (APPVER_HEXENREV < AV_HR_HEX10A) // Later moved (partially back) to I_IBM.C

#define DEFAULT_ARCHIVEPATH     "o:\\sound\\archive\\"
#define PRIORITY_MAX_ADJUST 10
#define DIST_ADJUST (MAX_SND_DIST/PRIORITY_MAX_ADJUST)

extern void **lumpcache;

extern sfxinfo_t S_sfx[];
extern musicinfo_t S_music[];

static channel_t Channel[MAX_CHANNELS];
static int RegisteredSong; //the current registered song.
static int NextCleanup;
static boolean MusicPaused;
static int Mus_Song = -1;
static int Mus_LumpNum;
static void *Mus_SndPtr;
static byte *SoundCurve;

static boolean UseSndScript;
static char ArchivePath[128];

#endif

// sound information
#if 0
const char *dnames[] = {"None",
			"PC_Speaker",
			"Adlib",
			"Sound_Blaster",
			"ProAudio_Spectrum16",
			"Gravis_Ultrasound",
			"MPU",
			"AWE32"
			};
#endif

#if (APPVER_HEXENREV >= AV_HR_HEX10A) // Possibly restored after moving code back to I_IBM.C
const char snd_prefixen[] = { 'P', 'P', 'A', 'S', 'S', 'S', 'M',
  'M', 'M', 'S' };
#endif

int snd_Channels;
int snd_DesiredMusicDevice, snd_DesiredSfxDevice;
int snd_MusicDevice,    // current music card # (index to dmxCodes)
	snd_SfxDevice,      // current sfx card # (index to dmxCodes)
	snd_MaxVolume,      // maximum volume for sound
	snd_MusicVolume;    // maximum volume for music
int dmxCodes[NUM_SCARDS]; // the dmx code for a given card

int     snd_SBport, snd_SBirq, snd_SBdma;       // sound blaster variables
int     snd_Mport;                              // midi variables

extern boolean  snd_MusicAvail, // whether music is available
		snd_SfxAvail;   // whether sfx are available

void I_PauseSong(int handle)
{
  MUS_PauseSong(handle);
}

void I_ResumeSong(int handle)
{
  MUS_ResumeSong(handle);
}

void I_SetMusicVolume(int volume)
{
  MUS_SetMasterVolume(volume*8);
//  snd_MusicVolume = volume;
}

void I_SetSfxVolume(int volume)
{
  snd_MaxVolume = volume; // THROW AWAY?
}

/*
 *
 *                              SONG API
 *
 */

int I_RegisterSong(void *data)
{
  int rc = MUS_RegisterSong(data);
#ifdef SNDDEBUG
  if (rc<0) ST_Message("    MUS_Reg() returned %d\n", rc);
#endif
  return rc;
}

void I_UnRegisterSong(int handle)
{
  int rc = MUS_UnregisterSong(handle);
#ifdef SNDDEBUG
  if (rc < 0) ST_Message("    MUS_Unreg() returned %d\n", rc);
#endif
}

int I_QrySongPlaying(int handle)
{
  int rc = MUS_QrySongPlaying(handle);
#ifdef SNDDEBUG
  if (rc < 0) ST_Message("    MUS_QrySP() returned %d\n", rc);
#endif
  return rc;
}

// Stops a song.  MUST be called before I_UnregisterSong().

void I_StopSong(int handle)
{
  int rc;
  rc = MUS_StopSong(handle);
#ifdef SNDDEBUG
  if (rc < 0) ST_Message("    MUS_StopSong() returned %d\n", rc);
#endif
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA) // Uncomment for the beta
  // Fucking kluge pause
  {
	int s;
	extern volatile int ticcount;
	for (s=ticcount ; ticcount - s < 10 ; );
  }
#endif
}

void I_PlaySong(int handle, boolean looping)
{
  int rc;
  rc = MUS_ChainSong(handle, looping ? handle : -1);
#ifdef SNDDEBUG
  if (rc < 0) ST_Message("    MUS_ChainSong() returned %d\n", rc);
#endif
  rc = MUS_PlaySong(handle, snd_MusicVolume);
#ifdef SNDDEBUG
  if (rc < 0) ST_Message("    MUS_PlaySong() returned %d\n", rc);
#endif

}

/*
 *
 *                                 SOUND FX API
 *
 */

// Gets lump nums of the named sound.  Returns pointer which will be
// passed to I_StartSound() when you want to start an SFX.  Must be
// sure to pass this to UngetSoundEffect() so that they can be
// freed!


int I_GetSfxLumpNum(sfxinfo_t *sound)
{
#if (APPVER_HEXENREV < AV_HR_HEX10A)
  if(UseSndScript)
	return -1;
  if(sound->lumpname == 0)
        return 0;
#endif
  return W_GetNumForName(sound->lumpname);

}

int I_StartSound (int id, void *data, int vol, int sep, int pitch, int priority)
{
	return SFX_PlayPatch(data, pitch, sep, vol, 0, 0);
}

#if (APPVER_HEXENREV < AV_HR_HEX10A)
int I_StartSoundWithImpact (int id, void *data, int vol, int sep, int pitch, int priority)
{
	return SFX_PlayPatch(data, pitch, sep, vol, 1, 0);
}

#endif

void I_StopSound(int handle)
{
//  extern volatile long gDmaCount;
//  long waittocount;
  SFX_StopPatch(handle);
//  waittocount = gDmaCount + 2;
//  while (gDmaCount < waittocount) ;
}

int I_SoundIsPlaying(int handle)
{
  return SFX_Playing(handle);
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
  SFX_SetOrigin(handle, pitch, sep, vol);
}

/*
 *
 *                                                      SOUND STARTUP STUFF
 *
 *
 */

//
// Why PC's Suck, Reason #8712
//

void I_sndArbitrateCards(void)
{
	char tmp[160];
  boolean gus, adlib, pc, sb, midi;
  int i, rc, mputype, p, opltype, wait, dmxlump;

  snd_MusicDevice = snd_DesiredMusicDevice;
  snd_SfxDevice = snd_DesiredSfxDevice;

  // check command-line parameters- overrides config file
  //
  if (M_CheckParm("-nosound")) snd_MusicDevice = snd_SfxDevice = snd_none;
  if (M_CheckParm("-nosfx")) snd_SfxDevice = snd_none;
  if (M_CheckParm("-nomusic")) snd_MusicDevice = snd_none;

  if (snd_MusicDevice > snd_MPU && snd_MusicDevice <= snd_MPU3)
	snd_MusicDevice = snd_MPU;
  if (snd_MusicDevice == snd_SB)
	snd_MusicDevice = snd_Adlib;
  if (snd_MusicDevice == snd_PAS)
	snd_MusicDevice = snd_Adlib;

  // figure out what i've got to initialize
  //
  gus = snd_MusicDevice == snd_GUS || snd_SfxDevice == snd_GUS;
  sb = snd_SfxDevice == snd_SB || snd_MusicDevice == snd_SB;
  adlib = snd_MusicDevice == snd_Adlib ;
  pc = snd_SfxDevice == snd_PC;
  midi = snd_MusicDevice == snd_MPU;

  // initialize whatever i've got
  //
  if (gus)
  {
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	if (GF1_Detect()) ST_Message("Dude.  The GUS ain't responding.\n");
#else
	if (GF1_Detect()) ST_Message("    Dude.  The GUS ain't responding.\n");
#endif
	else
	{
	  dmxlump = W_GetNumForName("dmxgus");
	  GF1_SetMap(W_CacheLumpNum(dmxlump, PU_CACHE), lumpinfo[dmxlump].size);
	}

  }
  if (sb)
  {
	if(debugmode)
	{
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	  ST_Message("cfg p=0x%x, i=%d, d=%d\n",
	  	snd_SBport, snd_SBirq, snd_SBdma);
#else
	  ST_Message("  Sound cfg p=0x%x, i=%d, d=%d\n",
	  	snd_SBport, snd_SBirq, snd_SBdma);
#endif
	}
	if (SB_Detect(&snd_SBport, &snd_SBirq, &snd_SBdma, 0))
	{
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	  ST_Message("SB isn't responding at p=0x%x, i=%d, d=%d\n",
	  	snd_SBport, snd_SBirq, snd_SBdma);
#else
	  ST_Message("    SB isn't responding at p=0x%x, i=%d, d=%d\n",
	  	snd_SBport, snd_SBirq, snd_SBdma);
#endif
	}
	else SB_SetCard(snd_SBport, snd_SBirq, snd_SBdma);

	if(debugmode)
	{
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	  ST_Message("SB_Detect returned p=0x%x,i=%d,d=%d\n",
	  	snd_SBport, snd_SBirq, snd_SBdma);
#else
	  ST_Message("    SB_Detect returned p=0x%x, i=%d, d=%d\n",
	  	snd_SBport, snd_SBirq, snd_SBdma);
#endif
	}
  }

  if (adlib)
  {
	if (AL_Detect(&wait,0))
	{
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	  	ST_Message("Dude.  The Adlib isn't responding.\n");
#else
	  	ST_Message("    Dude.  The Adlib isn't responding.\n");
#endif
	}
	else
	{
		AL_SetCard(wait, W_CacheLumpName("genmidi", PU_STATIC));
	}
  }

  if (midi)
  {
	if (debugmode)
	{
#if (APPVER_HEXENREV < AV_HR_HEX10A)
		ST_Message("cfg p=0x%x\n", snd_Mport);
#else
		ST_Message("    cfg p=0x%x\n", snd_Mport);
#endif
	}

	if (MPU_Detect(&snd_Mport, &i))
	{
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	  ST_Message("The MPU-401 isn't reponding @ p=0x%x.\n", snd_Mport);
#else
	  ST_Message("    The MPU-401 isn't reponding @ p=0x%x.\n", snd_Mport);
#endif
	}
	else MPU_SetCard(snd_Mport);
  }

}

// inits all sound stuff

void I_StartupSound (void)
{
#if (APPVER_HEXENREV < AV_HR_HEX10A) // This (and more) is from Heretic
	char tmp[80];
#endif
  int rc, i;

  if (debugmode)
#if (APPVER_HEXENREV >= AV_HR_HEX10A) && (APPVER_HEXENREV < AV_HR_HEX11A)
	ST_Message("  I_StartupSound: Hope you hear a pop.\n");
#else
	ST_Message("I_StartupSound: Hope you hear a pop.\n");
#endif

  // initialize dmxCodes[]
  dmxCodes[0] = 0;
  dmxCodes[snd_PC] = AHW_PC_SPEAKER;
  dmxCodes[snd_Adlib] = AHW_ADLIB;
  dmxCodes[snd_SB] = AHW_SOUND_BLASTER;
  dmxCodes[snd_PAS] = AHW_MEDIA_VISION;
  dmxCodes[snd_GUS] = AHW_ULTRA_SOUND;
  dmxCodes[snd_MPU] = AHW_MPU_401;
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
  dmxCodes[snd_MPU2] = AHW_MPU_401;
  dmxCodes[snd_MPU3] = AHW_MPU_401;
#endif
  dmxCodes[snd_AWE] = AHW_AWE32;
#if (APPVER_HEXENREV >= AV_HR_HEX10A)
  dmxCodes[snd_CDMUSIC] = 0;
#endif

  // inits sound library timer stuff
  I_StartupTimer();

  // pick the sound cards i'm going to use
  //
  I_sndArbitrateCards();

  if (debugmode)
  {
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	sprintf(tmp,"  Music device #%d & dmxCode=%d", snd_MusicDevice,
	  dmxCodes[snd_MusicDevice]);
	printf(tmp);
	sprintf(tmp,"  Sfx device #%d & dmxCode=%d\n", snd_SfxDevice,
	  dmxCodes[snd_SfxDevice]);
	printf(tmp);
#elif (APPVER_HEXENREV < AV_HR_HEX11A)
	ST_Message("    Music device #%d & dmxCode=%d", snd_MusicDevice,
	  dmxCodes[snd_MusicDevice]);
	ST_Message("    Sfx device #%d & dmxCode=%d\n", snd_SfxDevice,
	  dmxCodes[snd_SfxDevice]);
#else
	ST_Message("    Music device #%d & dmxCode=%d,", snd_MusicDevice,
	  dmxCodes[snd_MusicDevice]);
	ST_Message(" Sfx device #%d & dmxCode=%d\n", snd_SfxDevice,
	  dmxCodes[snd_SfxDevice]);
#endif
  }

  // inits DMX sound library
#if (APPVER_HEXENREV < AV_HR_HEX10A)
  ST_Message("  calling DMX_Init");
#elif (APPVER_HEXENREV < AV_HR_HEX11A)
  ST_Message("    calling DMX_Init...");
#else
  ST_Message("    Calling DMX_Init...");
#endif
  rc = DMX_Init(SND_TICRATE, SND_MAXSONGS, dmxCodes[snd_MusicDevice],
	dmxCodes[snd_SfxDevice]);

  if (debugmode)
  {
#if (APPVER_HEXENREV < AV_HR_HEX10A)
	sprintf(tmp,"  DMX_Init() returned %d", rc);
	printf(tmp);
#elif (APPVER_HEXENREV < AV_HR_HEX11A)
	ST_Message("    DMX_Init() returned %d\n", rc);
#else
	ST_Message(" DMX_Init() returned %d\n", rc);
#endif
  }

}

// shuts down all sound stuff

void I_ShutdownSound (void)
{
  DMX_DeInit();
  I_ShutdownTimer();
}

void I_SetChannels(int channels)
{
  WAV_PlayMode(channels, SND_SAMPLERATE);
}

#if (APPVER_HEXENREV < AV_HR_HEX10A) // Later moved (partially back) to I_IBM.C

//==========================================================================
//
// S_Start
//
//==========================================================================

void S_Start(void)
{
	int i;

	S_StartSong(gamemap, true);

	//stop all sounds
	for(i=0; i < snd_Channels; i++)
	{
		if(Channel[i].handle)
		{
			S_StopSound(Channel[i].mo);
		}
	}
	memset(Channel, 0, 8*sizeof(channel_t));
}

//==========================================================================
//
// S_StartSong
//
//==========================================================================

void S_StartSong(int song, boolean loop)
{
	char *songLump;
	int track;

	if(song == Mus_Song)
	{ // don't replay an old song
		return;
	}
	if(RegisteredSong)
	{
		I_StopSong(RegisteredSong);
		I_UnRegisterSong(RegisteredSong);
		if(UseSndScript)
		{
			Z_Free(Mus_SndPtr);
		}
		else
		{
			Z_ChangeTag(lumpcache[Mus_LumpNum], PU_CACHE);
		}
		#ifdef __WATCOMC__
			_dpmi_unlockregion(Mus_SndPtr, lumpinfo[Mus_LumpNum].size);
		#endif
		RegisteredSong = 0;
	}
	songLump = P_GetMapSongLump(song);
	if(!songLump)
	{
		return;
	}
	if(UseSndScript)
	{
		char name[128];
		sprintf(name, "%s%s.lmp", ArchivePath, songLump);
		M_ReadFile(name, (byte **)&Mus_SndPtr);
	}
	else
	{
		Mus_LumpNum = W_GetNumForName(songLump);
		Mus_SndPtr = W_CacheLumpNum(Mus_LumpNum, PU_MUSIC);
	}
	#ifdef __WATCOMC__
		_dpmi_lockregion(Mus_SndPtr, lumpinfo[Mus_LumpNum].size);
	#endif
	RegisteredSong = I_RegisterSong(Mus_SndPtr);
	I_PlaySong(RegisteredSong, loop); // 'true' denotes endless looping.
	Mus_Song = song;
}

//==========================================================================
//
// S_StartSongName
//
//==========================================================================

void S_StartSongName(char *songLump, boolean loop)
{
	int cdTrack;

	if(!songLump)
	{
		return;
	}
	if(RegisteredSong)
	{
		I_StopSong(RegisteredSong);
		I_UnRegisterSong(RegisteredSong);
		if(UseSndScript)
		{
			Z_Free(Mus_SndPtr);
		}
		else
		{
			Z_ChangeTag(lumpcache[Mus_LumpNum], PU_CACHE);
		}
		#ifdef __WATCOMC__
			_dpmi_unlockregion(Mus_SndPtr, lumpinfo[Mus_LumpNum].size);
		#endif
		RegisteredSong = 0;
	}
	if(UseSndScript)
	{
		char name[128];
		sprintf(name, "%s%s.lmp", ArchivePath, songLump);
		M_ReadFile(name, (byte **)&Mus_SndPtr);
	}
	else
	{
		Mus_LumpNum = W_GetNumForName(songLump);
		Mus_SndPtr = W_CacheLumpNum(Mus_LumpNum, PU_MUSIC);
	}
	#ifdef __WATCOMC__
		_dpmi_lockregion(Mus_SndPtr, lumpinfo[Mus_LumpNum].size);
	#endif
	RegisteredSong = I_RegisterSong(Mus_SndPtr);
	I_PlaySong(RegisteredSong, loop); // 'true' denotes endless looping.
	Mus_Song = -1;
}

#if (APPVER_HEXENREV < AV_HR_HEXDEMOA) // Partially based on Heretic code
void S_StartSound(mobj_t *origin, int sound_id)
{
	int dist, vol;
	int i;
	int sound;
	int priority;
	int sep;
	int angle;
	int absx;
	int absy;

	static int sndcount = 0;
	int chan;

	if(sound_id == 0 || snd_MaxVolume == 0)
		return;
	if(origin == NULL)
	{
		origin = players[consoleplayer].mo;
	}

	// calculate the distance before other stuff so that we can throw out
	// sounds that are beyond the hearing range.
	absx = abs(origin->x-players[consoleplayer].mo->x);
	absy = abs(origin->y-players[consoleplayer].mo->y);
	dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
	dist >>= FRACBITS;
	if(dist >= MAX_SND_DIST)
	{
	  return; // sound is beyond the hearing range...
	}
	if(dist < 0)
	{
		dist = 0;
	}
	priority = S_sfx[sound_id].priority;
	priority *= (PRIORITY_MAX_ADJUST-(dist/DIST_ADJUST));
	if(!S_StopSoundID(sound_id, priority))
	{
		return; // other sounds have greater priority
	}
	for(i=0; i<snd_Channels; i++)
	{
		if(origin->player)
		{
			i = snd_Channels;
			break; // let the player have more than one sound.
		}
		if(origin == Channel[i].mo)
		{ // only allow other mobjs one sound
			S_StopSound(Channel[i].mo);
			break;
		}
	}
	if(i >= snd_Channels)
	{
		for(i = 0; i < snd_Channels; i++)
		{
			if(Channel[i].mo == NULL)
			{
				break;
			}
		}
		if(i >= snd_Channels)
		{
			// look for a lower priority sound to replace.
			sndcount++;
			if(sndcount >= snd_Channels)
			{
				sndcount = 0;
			}
			for(chan = 0; chan < snd_Channels; chan++)
			{
				i = (sndcount+chan)%snd_Channels;
				if(priority >= Channel[i].priority)
				{
					chan = -1; //denote that sound should be replaced.
					break;
				}
			}
			if(chan != -1)
			{
				return; //no free channels.
			}
			else //replace the lower priority sound.
			{
				if(Channel[i].handle)
				{
					if(I_SoundIsPlaying(Channel[i].handle))
					{
						I_StopSound(Channel[i].handle);
					}
					if(S_sfx[Channel[i].sound_id].usefulness > 0)
					{
						S_sfx[Channel[i].sound_id].usefulness--;
					}
				}
			}
		}
	}
	if(S_sfx[sound_id].lumpnum == 0)
	{
		S_sfx[sound_id].lumpnum = I_GetSfxLumpNum(&S_sfx[sound_id]);
	}
	if(S_sfx[sound_id].snd_ptr == NULL)
	{
		if(UseSndScript)
		{
			char name[128];
			sprintf(name, "%s%s.lmp", ArchivePath, S_sfx[sound_id].lumpname);
			M_ReadFile(name, (byte **)&S_sfx[sound_id].snd_ptr);
		}
		else
		{
			S_sfx[sound_id].snd_ptr = W_CacheLumpNum(S_sfx[sound_id].lumpnum,
				PU_SOUND);
		}
		#ifdef __WATCOMC__
		_dpmi_lockregion(S_sfx[sound_id].snd_ptr,
			lumpinfo[S_sfx[sound_id].lumpnum].size);
		#endif
	}

	vol = SoundCurve[dist];
	if(origin == players[consoleplayer].mo)
	{
		sep = 128;
	}
	else
	{
		angle = R_PointToAngle2(players[consoleplayer].mo->x,
			players[consoleplayer].mo->y, Channel[i].mo->x, Channel[i].mo->y);
		angle = (angle-viewangle)>>24;
		sep = angle*2-128;
		if(sep < 64)
			sep = -sep;
		if(sep > 192)
			sep = 512-sep;
	}

	if(S_sfx[sound_id].changePitch)
	{
		Channel[i].pitch = (byte)(127+(M_Random()&7)-(M_Random()&7));
	}
	else
	{
		Channel[i].pitch = 127;
	}
	Channel[i].handle = I_StartSound(sound_id, S_sfx[sound_id].snd_ptr, vol,
		sep, Channel[i].pitch, 0x40);
	Channel[i].mo = origin;
	Channel[i].sound_id = sound_id;
	Channel[i].priority = priority;
	if(S_sfx[sound_id].usefulness < 0)
	{
		S_sfx[sound_id].usefulness = 1;
	}
	else
	{
		S_sfx[sound_id].usefulness++;
	}
}

void S_StartSoundAtVolumeOld(mobj_t *origin, int sound_id, int volume)
{
	int dist;
	int i;
	int sep;

	static int sndcount;
	int chan;

	if(sound_id == 0 || snd_MaxVolume == 0)
		return;
	if(origin == NULL)
	{
		origin = players[consoleplayer].mo;
	}

	if(volume == 0)
	{
		return;
	}
	volume = (volume*(snd_MaxVolume+1)*8)>>7;

// no priority checking, as ambient sounds would be the LOWEST.
	for(i=0; i<snd_Channels; i++)
	{
		if(Channel[i].mo == NULL)
		{
			break;
		}
	}
	if(i >= snd_Channels)
	{
		return;
	}
	if(S_sfx[sound_id].lumpnum == 0)
	{
		S_sfx[sound_id].lumpnum = I_GetSfxLumpNum(&S_sfx[sound_id]);
	}
	if(S_sfx[sound_id].snd_ptr == NULL)
	{
		if(UseSndScript)
		{
			char name[128];
			sprintf(name, "%s%s.lmp", ArchivePath, S_sfx[sound_id].lumpname);
			M_ReadFile(name, (byte **)&S_sfx[sound_id].snd_ptr);
		}
		else
		{
			S_sfx[sound_id].snd_ptr = W_CacheLumpNum(S_sfx[sound_id].lumpnum,
				PU_SOUND);
		}
		#ifdef __WATCOMC__
		_dpmi_lockregion(S_sfx[sound_id].snd_ptr,
			lumpinfo[S_sfx[sound_id].lumpnum].size);
		#endif
	}
	if(S_sfx[sound_id].changePitch)
	{
		Channel[i].pitch = (byte)(127-(M_Random()&3)+(M_Random()&3));
	}
	else
	{
		Channel[i].pitch = 127;
	}
	Channel[i].handle = I_StartSoundWithImpact(sound_id, S_sfx[sound_id].snd_ptr, volume, 128, Channel[i].pitch, 0);
	Channel[i].mo = origin;
	Channel[i].sound_id = sound_id;
	Channel[i].priority = 1; //super low priority.
	if(S_sfx[sound_id].usefulness < 0)
	{
		S_sfx[sound_id].usefulness = 1;
	}
	else
	{
		S_sfx[sound_id].usefulness++;
	}
}

int S_GetSoundID(char *name)
{
	int i;

	for(i = 0; i < NUMSFX; i++)
	{
		if(!strcmp(S_sfx[i].tagName, name))
		{
			return i;
		}
	}
	return 0;
}
#else // APPVER_HEXENREV >= AV_HR_HEXDEMOA
//==========================================================================
//
// S_GetSoundID
//
//==========================================================================

int S_GetSoundID(char *name)
{
	int i;

	for(i = 0; i < NUMSFX; i++)
	{
		if(!strcmp(S_sfx[i].tagName, name))
		{
			return i;
		}
	}
	return 0;
}

//==========================================================================
//
// S_StartSound
//
//==========================================================================


void S_StartSound(mobj_t *origin, int sound_id)
{
	S_StartSoundAtVolume(origin, sound_id, 127);
}
#endif // APPVER_HEXENREV

//==========================================================================
//
// S_StartSoundAtVolume
//
//==========================================================================

void S_StartSoundAtVolume(mobj_t *origin, int sound_id, int volume)
{
	int dist, vol;
	int i;
	int priority;
	int sep;
	int angle;
	int absx;
	int absy;

	static int sndcount = 0;
	int chan;

	if(sound_id == 0 || snd_MaxVolume == 0)
		return;
	if(origin == NULL)
	{
		origin = players[consoleplayer].mo;
	}
	if(volume == 0)
	{
		return;
	}

	// calculate the distance before other stuff so that we can throw out
	// sounds that are beyond the hearing range.
	absx = abs(origin->x-players[consoleplayer].mo->x);
	absy = abs(origin->y-players[consoleplayer].mo->y);
	dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
	dist >>= FRACBITS;
	if(dist >= MAX_SND_DIST)
	{
	  return; // sound is beyond the hearing range...
	}
	if(dist < 0)
	{
		dist = 0;
	}
	priority = S_sfx[sound_id].priority;
	priority *= (PRIORITY_MAX_ADJUST-(dist/DIST_ADJUST));
	if(!S_StopSoundID(sound_id, priority))
	{
		return; // other sounds have greater priority
	}
	for(i=0; i<snd_Channels; i++)
	{
		if(origin->player)
		{
			i = snd_Channels;
			break; // let the player have more than one sound.
		}
		if(origin == Channel[i].mo)
		{ // only allow other mobjs one sound
			S_StopSound(Channel[i].mo);
			break;
		}
	}
	if(i >= snd_Channels)
	{
		for(i = 0; i < snd_Channels; i++)
		{
			if(Channel[i].mo == NULL)
			{
				break;
			}
		}
		if(i >= snd_Channels)
		{
			// look for a lower priority sound to replace.
			sndcount++;
			if(sndcount >= snd_Channels)
			{
				sndcount = 0;
			}
			for(chan = 0; chan < snd_Channels; chan++)
			{
				i = (sndcount+chan)%snd_Channels;
				if(priority >= Channel[i].priority)
				{
					chan = -1; //denote that sound should be replaced.
					break;
				}
			}
			if(chan != -1)
			{
				return; //no free channels.
			}
			else //replace the lower priority sound.
			{
				if(Channel[i].handle)
				{
					if(I_SoundIsPlaying(Channel[i].handle))
					{
						I_StopSound(Channel[i].handle);
					}
					if(S_sfx[Channel[i].sound_id].usefulness > 0)
					{
						S_sfx[Channel[i].sound_id].usefulness--;
					}
				}
			}
		}
	}
	if(S_sfx[sound_id].lumpnum == 0)
	{
		S_sfx[sound_id].lumpnum = I_GetSfxLumpNum(&S_sfx[sound_id]);
	}
	if(S_sfx[sound_id].snd_ptr == NULL)
	{
		if(UseSndScript)
		{
			char name[128];
			sprintf(name, "%s%s.lmp", ArchivePath, S_sfx[sound_id].lumpname);
			M_ReadFile(name, (byte **)&S_sfx[sound_id].snd_ptr);
		}
		else
		{
			S_sfx[sound_id].snd_ptr = W_CacheLumpNum(S_sfx[sound_id].lumpnum,
				PU_SOUND);
		}
		#ifdef __WATCOMC__
		_dpmi_lockregion(S_sfx[sound_id].snd_ptr,
			lumpinfo[S_sfx[sound_id].lumpnum].size);
		#endif
	}

#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	vol = (SoundCurve[dist]*(snd_MaxVolume*8)*volume)>>14;
#endif
	if(origin == players[consoleplayer].mo)
	{
		sep = 128;
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA) // Uncomment for beta
                vol = (volume*(snd_MaxVolume+1)*8)>>7;
#endif
	}
	else
	{
		angle = R_PointToAngle2(players[consoleplayer].mo->x,
			players[consoleplayer].mo->y, Channel[i].mo->x, Channel[i].mo->y);
		angle = (angle-viewangle)>>24;
		sep = angle*2-128;
		if(sep < 64)
			sep = -sep;
		if(sep > 192)
			sep = 512-sep;
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA) // Uncomment for beta
                vol = SoundCurve[dist];
#endif
	}

	if(S_sfx[sound_id].changePitch)
	{
		Channel[i].pitch = (byte)(127+(M_Random()&7)-(M_Random()&7));
	}
	else
	{
		Channel[i].pitch = 127;
	}
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA)
	Channel[i].handle = I_StartSound(sound_id, S_sfx[sound_id].snd_ptr, vol,
		sep, Channel[i].pitch, 0x40);
#else
	Channel[i].handle = I_StartSound(sound_id, S_sfx[sound_id].snd_ptr, vol,
		sep, Channel[i].pitch, 0);
#endif
	Channel[i].mo = origin;
	Channel[i].sound_id = sound_id;
	Channel[i].priority = priority;
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	Channel[i].volume = volume;
#endif
	if(S_sfx[sound_id].usefulness < 0)
	{
		S_sfx[sound_id].usefulness = 1;
	}
	else
	{
		S_sfx[sound_id].usefulness++;
	}
}

//==========================================================================
//
// S_StopSoundID
//
//==========================================================================

boolean S_StopSoundID(int sound_id, int priority)
{
	int i;
	int lp; //least priority
	int found;

	if(S_sfx[sound_id].numchannels == -1)
	{
		return(true);
	}
	lp = -1; //denote the argument sound_id
	found = 0;
	for(i=0; i<snd_Channels; i++)
	{
		if(Channel[i].sound_id == sound_id && Channel[i].mo)
		{
			found++; //found one.  Now, should we replace it??
			if(priority >= Channel[i].priority)
			{ // if we're gonna kill one, then this'll be it
				lp = i;
				priority = Channel[i].priority;
			}
		}
	}
	if(found < S_sfx[sound_id].numchannels)
	{
		return(true);
	}
	else if(lp == -1)
	{
		return(false); // don't replace any sounds
	}
	if(Channel[lp].handle)
	{
		if(I_SoundIsPlaying(Channel[lp].handle))
		{
			I_StopSound(Channel[lp].handle);
		}
		if(S_sfx[Channel[lp].sound_id].usefulness > 0)
		{
			S_sfx[Channel[lp].sound_id].usefulness--;
		}
		Channel[lp].mo = NULL;
	}
	return(true);
}

//==========================================================================
//
// S_StopSound
//
//==========================================================================

void S_StopSound(mobj_t *origin)
{
	int i;

	for(i=0;i<snd_Channels;i++)
	{
		if(Channel[i].mo == origin)
		{
			I_StopSound(Channel[i].handle);
			if(S_sfx[Channel[i].sound_id].usefulness > 0)
			{
				S_sfx[Channel[i].sound_id].usefulness--;
			}
			Channel[i].handle = 0;
			Channel[i].mo = NULL;
		}
	}
}

//==========================================================================
//
// S_SoundLink
//
//==========================================================================

void S_SoundLink(mobj_t *oldactor, mobj_t *newactor)
{
	int i;

	for(i=0;i<snd_Channels;i++)
	{
		if(Channel[i].mo == oldactor)
			Channel[i].mo = newactor;
	}
}

//==========================================================================
//
// S_PauseSound
//
//==========================================================================

void S_PauseSound(void)
{
	I_PauseSong(RegisteredSong);
}

//==========================================================================
//
// S_ResumeSound
//
//==========================================================================

void S_ResumeSound(void)
{
	I_ResumeSong(RegisteredSong);
}

//==========================================================================
//
// S_UpdateSounds
//
//==========================================================================

void S_UpdateSounds(mobj_t *listener)
{
	int i, dist, vol;
	int angle;
	int sep;
	int priority;
	int absx;
	int absy;

	listener = players[consoleplayer].mo;
	if(snd_MaxVolume == 0)
	{
		return;
	}

	// Update any Sequences
	SN_UpdateActiveSequences();

	if(NextCleanup < gametic)
	{
		if(UseSndScript)
		{
			for(i = 0; i < NUMSFX; i++)
			{
				if(S_sfx[i].usefulness == 0 && S_sfx[i].snd_ptr)
				{
					S_sfx[i].usefulness = -1;
				}
			}
		}
		else
		{
			for(i = 0; i < NUMSFX; i++)
			{
				if(S_sfx[i].usefulness == 0 && S_sfx[i].snd_ptr)
				{
					if(lumpcache[S_sfx[i].lumpnum])
					{
						if(((memblock_t *)((byte*)
							(lumpcache[S_sfx[i].lumpnum])-
							sizeof(memblock_t)))->id == 0x1d4a11)
						{ // taken directly from the Z_ChangeTag macro
							Z_ChangeTag2(lumpcache[S_sfx[i].lumpnum],
								PU_CACHE);
#ifdef __WATCOMC__
								_dpmi_unlockregion(S_sfx[i].snd_ptr,
									lumpinfo[S_sfx[i].lumpnum].size);
#endif
						}
					}
					S_sfx[i].usefulness = -1;
					S_sfx[i].snd_ptr = NULL;
				}
			}
		}
		NextCleanup = gametic+35*30; // every 30 seconds
	}
	for(i=0;i<snd_Channels;i++)
	{
		if(!Channel[i].handle || S_sfx[Channel[i].sound_id].usefulness == -1)
		{
			continue;
		}
		if(!I_SoundIsPlaying(Channel[i].handle))
		{
			if(S_sfx[Channel[i].sound_id].usefulness > 0)
			{
				S_sfx[Channel[i].sound_id].usefulness--;
			}
			Channel[i].handle = 0;
			Channel[i].mo = NULL;
			Channel[i].sound_id = 0;
		}
		if(Channel[i].mo == NULL || Channel[i].sound_id == 0
			|| Channel[i].mo == players[consoleplayer].mo)
		{
			continue;
		}
		else
		{
			absx = abs(Channel[i].mo->x-players[consoleplayer].mo->x);
			absy = abs(Channel[i].mo->y-players[consoleplayer].mo->y);
			dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
			dist >>= FRACBITS;

			if(dist >= MAX_SND_DIST)
			{
				S_StopSound(Channel[i].mo);
				continue;
			}
			if(dist < 0)
			{
				dist = 0;
			}
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA) // Uncomment simple assignment for beta
			vol = SoundCurve[dist];
#else
			vol = (SoundCurve[dist]*(snd_MaxVolume*8)*Channel[i].volume)>>14;
			if(Channel[i].mo == players[consoleplayer].mo)
			{
				sep = 128;
			}
			else
#endif
			{
				angle = R_PointToAngle2(players[consoleplayer].mo->x, players[consoleplayer].mo->y,
								Channel[i].mo->x, Channel[i].mo->y);
				angle = (angle-viewangle)>>24;
				sep = angle*2-128;
				if(sep < 64)
					sep = -sep;
				if(sep > 192)
					sep = 512-sep;
			}
			I_UpdateSoundParams(Channel[i].handle, vol, sep,
				Channel[i].pitch);
			priority = S_sfx[Channel[i].sound_id].priority;
			priority *= PRIORITY_MAX_ADJUST-(dist/DIST_ADJUST);
			Channel[i].priority = priority;
		}
	}
}

//==========================================================================
//
// S_Init
//
//==========================================================================

void S_Init(void)
{
#if (APPVER_HEXENREV >= AV_HR_HEXDEMOA)
	SoundCurve = W_CacheLumpName("SNDCURVE", PU_STATIC);
#else // From Heretic; Uncomment line for earlier version
        SoundCurve = Z_Malloc(MAX_SND_DIST, PU_STATIC, NULL);
#endif
	I_StartupSound();
	if(snd_Channels > 8)
	{
		snd_Channels = 8;
	}
	I_SetChannels(snd_Channels);
	I_SetMusicVolume(snd_MusicVolume);
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA) // From Heretic again
	S_SetMaxVolume(true);
#endif
}

//==========================================================================
//
// S_GetChannelInfo
//
//==========================================================================

void S_GetChannelInfo(SoundInfo_t *s)
{
	int i;
	ChanInfo_t *c;

	s->channelCount = snd_Channels;
	s->musicVolume = snd_MusicVolume;
	s->soundVolume = snd_MaxVolume;
	for(i = 0; i < snd_Channels; i++)
	{
		c = &s->chan[i];
		c->id = Channel[i].sound_id;
		c->priority = Channel[i].priority;
		c->name = S_sfx[c->id].lumpname;
		c->mo = Channel[i].mo;
		c->distance = P_AproxDistance(c->mo->x-viewx, c->mo->y-viewy)
			>>FRACBITS;
	}
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
//==========================================================================

boolean S_GetSoundPlayingInfo(mobj_t *mobj, int sound_id)
{
	int i;

	for(i = 0; i < snd_Channels; i++)
	{
		if(Channel[i].sound_id == sound_id && Channel[i].mo == mobj)
		{
			if(I_SoundIsPlaying(Channel[i].handle))
			{
				return true;
			}
		}
	}
	return false;
}

#if (APPVER_HEXENREV < AV_HR_HEX10A) // Originally from Heretic, later removed
void S_SetMaxVolume(boolean fullprocess)
{
#if (APPVER_HEXENREV < AV_HR_HEXDEMOA)
	int i;

	if(!fullprocess)
	{
		SoundCurve[0] = (*((byte *)W_CacheLumpName("SNDCURVE", PU_CACHE))*(snd_MaxVolume*8))>>7;
	}
	else
	{
		for(i = 0; i < MAX_SND_DIST; i++)
		{
			SoundCurve[i] = (*((byte *)W_CacheLumpName("SNDCURVE", PU_CACHE)+i)*(snd_MaxVolume*8))>>7;
		}
	}
#endif
}
#endif

//==========================================================================
//
// S_SetMusicVolume
//
//==========================================================================

void S_SetMusicVolume(void)
{
	I_SetMusicVolume(snd_MusicVolume);
	if(snd_MusicVolume == 0)
	{
		I_PauseSong(RegisteredSong);
		MusicPaused = true;
	}
	else if(MusicPaused)
	{
		MusicPaused = false;
		I_ResumeSong(RegisteredSong);
	}
}

//==========================================================================
//
// S_ShutDown
//
//==========================================================================

void S_ShutDown(void)
{
	extern int tsm_ID;
	if(tsm_ID != -1)
	{
		I_StopSong(RegisteredSong);
		I_UnRegisterSong(RegisteredSong);
		I_ShutdownSound();
	}
}

//==========================================================================
//
// S_InitScript
//
//==========================================================================

void S_InitScript(void)
{
	int p;
	int i;

	strcpy(ArchivePath, DEFAULT_ARCHIVEPATH);
	if(!(p = M_CheckParm("-devsnd")))
	{
		UseSndScript = false;
		SC_OpenLump("sndinfo");
	}
	else
	{
		UseSndScript = true;
		SC_OpenFile(myargv[p+1]);
	}
	while(SC_GetString())
	{
		if(*sc_String == '$')
		{
			if(!stricmp(sc_String, "$ARCHIVEPATH"))
			{
				SC_MustGetString();
				strcpy(ArchivePath, sc_String);
			}
			else if(!stricmp(sc_String, "$MAP"))
			{
				SC_MustGetNumber();
				SC_MustGetString();
				if(sc_Number)
				{
					P_PutMapSongLump(sc_Number, sc_String);
				}
			}
			continue;
		}
		else
		{
			for(i = 0; i < NUMSFX; i++)
			{
				if(!strcmp(S_sfx[i].tagName, sc_String))
				{
					SC_MustGetString();
					if(*sc_String != '?')
					{
						strcpy(S_sfx[i].lumpname, sc_String);
					}
					else
					{
						strcpy(S_sfx[i].lumpname, "default");
					}
					break;
				}
			}
			if(i == NUMSFX)
			{
				SC_MustGetString();
			}
		}
	}
	SC_Close();

	for(i = 0; i < NUMSFX; i++)
	{
		if(!strcmp(S_sfx[i].lumpname, ""))
		{
			strcpy(S_sfx[i].lumpname, "default");
		}
	}
}
#endif // APPVER_HEXENREV < AV_HR_HEX10A, later moved (partially back) to I_IBM.C
