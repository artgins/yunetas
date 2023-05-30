/***********************************************************************
 *              ANSI_ESCAPE_CODES.H
 *              Copyright (c) 2019 Niyamaka.
 *              All Rights Reserved.
 *              Own type definitions
 ***********************************************************************/
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************************
 *              Constants
 ***************************************************************************/
// Reset
#define Color_Off "\033[0m"       // Text Reset

// Regular Colors
#define RBlack "\033[0;30m"        // Black
#define RRed "\033[0;31m"          // Red
#define RGreen "\033[0;32m"        // Green
#define RYellow "\033[0;33m"       // Yellow
#define RBlue "\033[0;34m"         // Blue
#define RPurple "\033[0;35m"       // Purple
#define RCyan "\033[0;36m"         // Cyan
#define RWhite "\033[0;37m"        // White

// Bold
#define BBlack "\033[1;30m"       // Black
#define BRed "\033[1;31m"         // Red
#define BGreen "\033[1;32m"       // Green
#define BYellow "\033[1;33m"      // Yellow
#define BBlue "\033[1;34m"        // Blue
#define BPurple "\033[1;35m"      // Purple
#define BCyan "\033[1;36m"        // Cyan
#define BWhite "\033[1;37m"       // White

// Underline
#define UBlack "\033[4;30m"       // Black
#define URed "\033[4;31m"         // Red
#define UGreen "\033[4;32m"       // Green
#define UYellow "\033[4;33m"      // Yellow
#define UBlue "\033[4;34m"        // Blue
#define UPurple "\033[4;35m"      // Purple
#define UCyan "\033[4;36m"        // Cyan
#define UWhite "\033[4;37m"       // White

// Background
#define On_Black "\033[40m"       // Black
#define On_Red "\033[41m"         // Red
#define On_Green "\033[42m"       // Green
#define On_Yellow "\033[43m"      // Yellow
#define On_Blue "\033[44m"        // Blue
#define On_Purple "\033[45m"      // Purple
#define On_Cyan "\033[46m"        // Cyan
#define On_White "\033[47m"       // White

// High Intensity
#define IBlack "\033[0;90m"       // Black
#define IRed "\033[0;91m"         // Red
#define IGreen "\033[0;92m"       // Green
#define IYellow "\033[0;93m"      // Yellow
#define IBlue "\033[0;94m"        // Blue
#define IPurple "\033[0;95m"      // Purple
#define ICyan "\033[0;96m"        // Cyan
#define IWhite "\033[0;97m"       // White

// Bold High Intensity
#define BIBlack "\033[1;90m"      // Black
#define BIRed "\033[1;91m"        // Red
#define BIGreen "\033[1;92m"      // Green
#define BIYellow "\033[1;93m"     // Yellow
#define BIBlue "\033[1;94m"       // Blue
#define BIPurple "\033[1;95m"     // Purple
#define BICyan "\033[1;96m"       // Cyan
#define BIWhite "\033[1;97m"      // White

// High Intensity backgrounds
#define On_IBlack "\033[0;100m"   // Black
#define On_IRed "\033[0;101m"     // Red
#define On_IGreen "\033[0;102m"   // Green
#define On_IYellow "\033[0;103m"  // Yellow
#define On_IBlue "\033[0;104m"    // Blue
#define On_IPurple "\033[0;105m"  // Purple
#define On_ICyan "\033[0;106m"    // Cyan
#define On_IWhite "\033[0;107m"   // White

#define Erase_Whole_Line    "\033[2K"
#define Move_Horizontal     "\033[%dG"
#define Clear_Screen        "\033[2J"

#ifdef __cplusplus
}
#endif
