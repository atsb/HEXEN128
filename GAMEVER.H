// *** VERSIONS RESTORATION ***
#ifndef GAMEVER_H
#define GAMEVER_H

// It is assumed here that:
// 1. The compiler is set up to appropriately define APPVER_EXEDEF
// as an EXE identifier.
// 2. This header is included (near) the beginning of every compilation unit,
// in order to have an impact in any place where it's expected to.

// APPVER_HEXENREV definitions
#define AV_HR_HEXRBETA 199509260
#define AV_HR_HEXDEMOA 199510020
#define AV_HR_HEX10A 199510130
#define AV_HR_HEX10B 199510150
#define AV_HR_HEXDEMOB 199510170
#define AV_HR_HEX11A 199603120
#define AV_HR_HEX11B 199603220

// Now define APPVER_HEXENREV to one of the above, based on APPVER_EXEDEF

#define APPVER_CONCAT1(x,y) x ## y
#define APPVER_CONCAT2(x,y) APPVER_CONCAT1(x,y)
#define APPVER_HEXENREV APPVER_CONCAT2(AV_HR_,APPVER_EXEDEF)

// Conditionally define a separate macro for demo versions
#if (APPVER_HEXENREV == AV_HR_HEXDEMOA) || (APPVER_HEXENREV == AV_HR_HEXDEMOB)
#define APPVER_DEMO
#endif

#endif // GAMEVER_H
