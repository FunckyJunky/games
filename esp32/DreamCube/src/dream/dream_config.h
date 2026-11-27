// dream_config.h: DREAM-CUBE's name, version and update address (core/config.h includes this when it's there)
#pragma once
#define DREAM_CUBE 1
#define FW_NAME "DREAM-CUBE"
#define CUBE_VERSION "1.4"
#define FW_INFO_PATH "/dream/flash/firmware.json"     // newest DREAM-CUBE: {ver, path, size, md5, notes}
#define STT_PATH "/api/stt"                           // error.blue: speech to text (src/stt.js, Whisper)
#define DREAM_PATH "/api/dream"                       // error.blue: words -> a 3D scene (src/dream.js)
#define LINK_PATH "/api/dream/cube"                   // error.blue: the control page's link (src/dream.js)
