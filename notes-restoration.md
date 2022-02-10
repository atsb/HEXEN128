## WARNING: DO NOT TRY TOO HARD TO BUILD ANY OF THE ORIGINAL EXECUTABLES! ##

Please remember that any little difference, not matter how small it is,
can lead to a vastly different EXE layout. This includes differences in:

- The development tools (or parts of such); For instance, a compiler, a linker,
an assembler, or even a support library or header. A version number is not
a promise for having the exact tool used to reproduce some executable.
- The order in which source code files are listed in a given project or
makefile.
- Project settings.
- Source file specific settings in such a project.
- The order in which object or library files are passed to a linker.
- Any modification to a source code file (especially a header file).
- More than any of the above.

Following the warning, a description of the ways in which the executables were
reproduced is given.

With the right tools, this patched codebase can be used
for reproducing the executables coming from the following
original releases of "Hexen: Beyond Heretic", with a few caveats:

- Retail store beta (Sep 26 1995).
- Two 4 level demo versions.
- Versions 1.0 and 1.1, with two minor variations of each version.

The MAKEFILE bundled with the Hexen sources was used as a base. You
shall *not* call "wmake" or enter "M F". Instead, use DOBUILD.BAT.

List of releases by directory names
-----------------------------------

- HEXRBETA: "Hexen: Beyond Heretic" retail store beta, Sep 26th 1995 build.
- HEXDEMOA: "Hexen: Beyond Heretic" 4 level demo, Oct 2nd 1995 beta release.
- HEX10A: "Hexen: Beyond Heretic" early v1.0.
- HEX10B: "Hexen: Beyond Heretic" late v1.0 (with level load song bugfix).
- HEXDEMOB: "Hexen: Beyond Heretic" 4 level demo, Oct 18th 1995 re-release.
- HEX11A: "Hexen: Beyond Heretic" early v1.1 (as available from Steam).
- HEX11B: "Hexen: Beyond Heretic" late v1.1 (with A_SoAExplode bugfix for DK).

Technical details of the original EXEs (rather than any recreated one)
----------------------------------------------------------------------

|      Version      | N. bytes |               MD5                |  CRC-32  |
|-------------------|----------|----------------------------------|----------|
| Retail store beta |  918399  | 3a6603328f832314a4054e005da087c6 | 693e324b |
| Demo beta         |  856845  | c2dda1704b8e302e05c26f69433729f3 | 0452fae8 |
| 1.0               |  931829  | 08f08cad60c899cdce784c3adf6c5a6a | 92d2fd31 |
| 1.0 with fix      |  931829  | 435fd8c4770edf31b62bebd64aae9332 | 32d27bdf |
| Demo re-release   |  875885  | 458d3ff08d32fc50abb55a5b68660b6b | 031d5117 |
| 1.1               |  923019  | 713319e8adbc34bca8e06dbaff96f86c | bbc55df2 |
| 1.1 with fix      |  923019  | dfe619e8c6e3339359d62cda11e5375b | 741bb161 |

How to identify code changes (and what's this HEXENREV thing)?
--------------------------------------------------------------

Check out GAMEVER.H. Basically, for each EXE being built, the
macro APPVER_EXEDEF should be defined accordingly. For instance,
when building HEX10B, APPVER_EXEDEF is defined to be HEX10B.

Note that only C sources (and not ASM) are covered by the above, although
the compiled I_IBM_A.ASM and LINEAR.ASM code seems to be mostly identical
for all covered versions. In case any of the beta and/or demo versions
is built, TASM is instructed to define a macro which appropriately
impacts I_IBM_A.ASM:I_ReadJoystick.

Other than GAMEVER.H, the APPVER_EXEDEF macro is not used *anywhere*.
Instead, other macros are used, mostly APPVER_HEXENREV.

Any new macro may also be introduced if useful.

APPVER_HEXENREV is defined in all builds, with different values. It is
intended to represent a revision of development of the Hexen codebase.
Usually, this revision value is based on some evidenced date, or alternatively
a *guessed* date (say, an original modification date of the EXE).
Any other case is also a possibility.

These are two good reasons for using HEXENREV as described above, referring
to similar work done for Wolfenstein 3D EXEs (built with Borland C++):

- WL1AP12 and WL6AP11 share the same code revision. However, WL1AP11
is of an earlier revision. Thus, the usage of WOLFREV can be
less confusing.
- WOLFREV is a good way to describe the early March 1992 build. While
it's commonly called just "alpha" or "beta", GAMEVER_WOLFREV
gives us a more explicit description.

Is looking for "#if (APPVER_HEXENREV <= AV_HR_...)" (or >) sufficient?
----------------------------------------------------------------------

Nope!

Examples from Wolf3D/SOD:

For a project with GAMEVER_WOLFREV == GV_WR_SDMFG10,
the condition GAMEVER_WOLFREV <= GV_WR_SDMFG10 holds.
However, so does GAMEVER_WOLFREV <= GV_WR_WJ6IM14,
and also GAMEVER_WOLFREV > GV_WR_WL1AP10.
Furthermore, PML_StartupXMS (ID_PM.C) has two mentions of a bug fix
dating 10/8/92, for which the GAMEVER_WOLFREV test was chosen
appropriately. The exact range of WOLFREV values from this test
is not based on any specific build/release of an EXE.

What is this based on
---------------------

This codebase was originally based on the open-source release of Hexen,
which turned out to match a later revision identified as "1.1".
Multiple code pieces were brought over from the similar release
of the Heretic sources, or at least used as a reference.

Special thanks go to John Romero, Simon Howard, Mike Swanson, Frank Sapone,
Nuke.YKT, Evan Ramos and anybody else who deserves getting thanks.

What is *not* included
----------------------

As with Raven's original open source release, you won't find any of the files
from the DMX sound library. They're still required for making the EXEs in
such a way that their layouts will be as close to the originals' as possible.

Alternatively, to make a functioning EXE consisting of GPL-compatible sound
code for the purpose of having a test playthrough, you can use a replacement.
One which may currently be used is the Apogee Sound System backed DMX wrapper.
As expected, it'll sound different, especially the music (excluding CD Audio).

Building any of the EXEs
========================

Required tools:

- For the demo re-release: Watcom C 10.0a only; Not even 10.0 or 10.0b.
- For all other game versions: Watcom C 10.0 and exactly this version.
- For all game versions: Turbo Assembler 3.1.

Additionally:

- For mostly proper EXE layouts (albeit still with a few exceptions),
the dmx33gs headers and the dmx34a library should be used.
- Alternatively, if you just want to get an EXE with entirely GPL-compatible
code for checking the game (e.g., watching the demos), you can use the
Apogee Sound System backed DMX wrapper. In such a case,
the Apogee Sound System v1.09 will also be used.

Notes before trying to build anything:

- As previously mentioned, the EXEs will surely have great
differences in the layouts without access to the original DMX files.
- Even with access to the correct versions of DMX, Watcom C and
Turbo Assembler, this may depend on luck. In fact, in the case
of Hexen versions preceding 1.1, it is known that a relatively
large portion of the global variables, with emphasis on ones
from the BSS, will be ordered in a different manner within the EXE.
- There will also be minor differences stemming from the use
of the __LINE__ macro within I_Error.
- Depending on the way the compiler generates the code, the output
might still differ from the original for a few functions. In fact, the
compiler may generate the contents of a function body a bit differently
if an additional environment variable is defined, or alternatively, if
a macro definition is added. S_StartSoundAtVolume and M_FindResponseFile are
two examples of functions for which unexpected layout changes were observed.

Building any of the Hexen EXEs
==============================

1. Use DOBUILD.BAT, selecting the output directory name (say HEX10B),
depending on the EXE that you want to build. In case you want to use
the DMX wrapper, enter "DOBUILD.BAT USE_APODMX".
2. Hopefully you should get an EXE essentially behaving like the original,
with a possible exception for the DMX sound code.

Expected differences from the original (even if matching DMX files are used):

- A few unused gaps, mostly between C string literals, seem to be filled
with values depending on the environment while running the Watcom C compiler
(e.g., the exact contents of each compilation unit). This seems to be related
to Watcom C 10.0b and earlier versions, and less to 10.5, 10.6 or 11.0.
- The created STRIPHEX.EXE file will require an external DOS4GW EXE
(or compatible). You may optionally use DOS/4GW Professional to bind
its loader to the EXE, but inspection of the EXE layout wasn't done.
- As stated above, there are a few mentions of the __LINE__ macro via I_Error,
which will translate to numbers differing from the originals in a portion
of the cases. Additionally, for Hexen versions preceding 1.1, a subset of
the global variables in the BSS will be ordered in a different manner.
There might also be other differences which were missed.
- Furthermore, again as stated earlier, the compiler might generate
a bit different code for specific function bodies. In particular,
even with a proper setup that uses the expected DMX files,
there might be differences in the bodies of the functions
A_Quake, P_XYMovement and P_ZMovement for the 4-level beta.
Unless an actual difference in behaviors was missed, these
aren't examples stemming from such differences.
