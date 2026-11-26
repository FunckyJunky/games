// ear_config.h: EAR-CUBE's name, version and update address (core/config.h includes this when it's there)
#pragma once
#define EAR_CUBE 1
#define FW_NAME "EAR-CUBE"
#define CUBE_VERSION "1.2"
#define FW_INFO_PATH "/ear/flash/firmware.json"       // newest EAR-CUBE: {ver, path, size, md5, notes}
#define STT_PATH "/api/stt"                           // error.blue: speech to text (src/stt.js, Whisper on Cloudflare)
