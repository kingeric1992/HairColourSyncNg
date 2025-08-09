//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by version.rc

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        101
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1001
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif

#define VERSION_MAJOR 1
#define VERSION_MINOR 1
#define VERSION_PATCH 1
#define VERSION_BUILD 0

#define AUTHOR "kingeric1992"
#define EMAIL "king_eric1992@hotmail.com"
#define FILE_DESC "A SKSE64" DEBUG_STR " plugin to sync wig color to hair. NGify from https://github.com/jgernandt/skyrim-hair-colour-sync"
#define PLUGIN_NAME "HairColourSyncNG" DEBUG_STR

#define stringify(a) stringify_(a)
#define stringify_(a) #a

#if defined(_DEBUG) // .rc doesn't work with #ifdef
#define DEBUG_STR " Debug"
#else
#define DEBUG_STR ""
#endif

#define VERSION_STRING stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH)
#define VERSION_STRINGEX VERSION_STRING "." stringify(VERSION_BUILD)
#define MAKE_EXE_VERSION_EX(major, minor, build, sub)	((((major) & 0xFF) << 24) | (((minor) & 0xFF) << 16) | (((sub) & 0xF) << 12) | (((build) & 0xFFF) << 0))

#define PACKED_VERSION MAKE_EXE_VERSION_EX(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_BUILD)
