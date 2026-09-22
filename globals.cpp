#include "globals.h"
#include "config.h"

SPIClass tftSPI(VSPI);
SPIClass sdSPI(HSPI);
AnimatedGIF gif;

String manifestItems[MAX_ITEMS];
int    manifestCount = 0;

String g_currentBasename = "";
bool   g_gifNeedsReload  = true;
bool   gifReady          = false;
bool   gifEverOpened     = false;

File   gifFile;
String currentGifPath;

QueueHandle_t     audioCmdQueue = nullptr;
SemaphoreHandle_t sdMutex       = nullptr;
