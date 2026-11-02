#pragma once
#ifdef BUILD_A_EMBEDDED
#define BA_PREFERENCE(name) "ux0:/data/RS2Vita/" name
#else
#define BA_PREFERENCE(name) name
#endif
