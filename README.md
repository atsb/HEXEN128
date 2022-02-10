# HEXEN128
A HEXEN extension for DOS using increased limits beyond what vanilla and executable hacks provide.

This affects late version 1.1 of HEXEN Other versions can be built if wanted.

# Why
Because having a DOS compatible HEXEN source port that increases the limits beyond what various executable hacks does is useful in playing modern WADS on DOS (where possible).

HEXEN128 is a friendly fork of the amazing restoration work done here:
https://bitbucket.org/gamesrc-ver-recreation/hexen/src/master/

In short, it is about as vanilla as you will get and is compiled using the correct WATCOM version.

Please note, HEX128 binaries are built using the original DMX library.  You won't be able to build it if you don't have it or don't use the APODMX wrapper (see the gamesrc recreation link).

# Limits
MAXVISSPRITES    1024 * 16

SAVESTRINGSIZE 32

MAXLINEANIMS        16384 * 16

MAXPLATS    7680 * 16

MAXVISPLANES    1024 * 16

MAXOPENINGS        SCREENWIDTH*256 * 16

MAXDRAWSEGS        2048 * 16

MAXSEGS (SCREENWIDTH / 2 + 1) * SCREENHEIGHT

SAVEGAMESIZE 0x20000 * 16
