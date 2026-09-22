#include "sd_manifest.h"
#include "globals.h"
#include "config.h"

// ====================================================================
// HELPER STRING
// ====================================================================

static String stripExtAndPath(String name) {
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);
  int dot = name.lastIndexOf('.');
  if (dot > 0) name = name.substring(0, dot);
  return name;
}

static bool endsWithIgnoreCase(const String &name, const char* ext) {
  String lower = name;        lower.toLowerCase();
  String extLo = String(ext); extLo.toLowerCase();
  return lower.endsWith(extLo);
}

static int scanDirBasenames(const char* dirPath, const char* ext, String* out, int maxOut) {
  int n = 0;
  File dir = SD.open(dirPath);

  if (!dir || !dir.isDirectory()) {
    Serial.printf("Directory not found: %s\n", dirPath);
    return 0;
  }

  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String nm = String(f.name());
      if (endsWithIgnoreCase(nm, ext)) {
        if (n < maxOut) {
          out[n++] = stripExtAndPath(nm);
        } else {
          Serial.println("WARNING: MAX_ITEMS exceeded");
        }
      }
    }
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  return n;
}

// ====================================================================
// SD INIT
// ====================================================================

bool sdInit() {
  Serial.println("Initializing SD Card...");
  Serial.flush();

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  bool sdOK = false;
  for (int attempt = 1; attempt <= 3; attempt++) {
    Serial.printf("  attempt %d...\n", attempt);
    Serial.flush();

    if (SD.begin(SD_CS, sdSPI, 4000000)) {
      sdOK = true;
      break;
    }
    delay(500);
  }

  if (!sdOK) {
    Serial.println("SD Card FAILED after 3 attempts. Check wiring/format (must be FAT32).");
    return false;
  }

  Serial.println("SD Card OK");

  if (!SD.exists(ROOT_DIR))  SD.mkdir(ROOT_DIR);
  if (!SD.exists(GIF_DIR))   SD.mkdir(GIF_DIR);
  if (!SD.exists(AUDIO_DIR)) SD.mkdir(AUDIO_DIR);

  return true;
}

// ====================================================================
// MANIFEST: SCAN & PAIR gif+wav BASED ON NAME
// ====================================================================

void buildManifest() {
  static String gifs[MAX_ITEMS];
  static String auds[MAX_ITEMS];

  int gCount = scanDirBasenames(GIF_DIR,   ".gif", gifs, MAX_ITEMS);
  int aCount = scanDirBasenames(AUDIO_DIR, ".wav", auds, MAX_ITEMS);

  int count = 0;
  for (int i = 0; i < gCount; i++) {
    for (int j = 0; j < aCount; j++) {
      if (gifs[i].equalsIgnoreCase(auds[j])) {
        if (count < MAX_ITEMS) manifestItems[count++] = gifs[i];
        break;
      }
    }
  }

  manifestCount = count;
  Serial.printf("Scan: %d gif, %d wav found, %d matching pairs\n",
      gCount, aCount, count);

  if (manifestCount == 0) {
    Serial.println("WARNING: no matching gif+wav pairs found!");
  }
}

bool writeManifestJSON() {
  if (SD.exists(MANIFEST_PATH)) SD.remove(MANIFEST_PATH);

  File f = SD.open(MANIFEST_PATH, FILE_WRITE);
  if (!f) {
    Serial.println("Failed to create manifest.json");
    return false;
  }

  f.print("{\n  \"count\": ");
  f.print(manifestCount);
  f.print(",\n  \"items\": [\n");

  for (int i = 0; i < manifestCount; i++) {
    f.print("    \"");
    f.print(manifestItems[i]);
    f.print("\"");
    if (i < manifestCount - 1) f.print(",");
    f.print("\n");
  }

  f.print("  ]\n}\n");
  f.close();

  Serial.println("manifest.json written to SD card");
  return true;
}

// Minimal parser for our own manifest format (not a general JSON parser):
// take all strings in quotes, discard keys "count" and "items".
bool loadManifestJSON() {
  File f = SD.open(MANIFEST_PATH, FILE_READ);
  if (!f) {
    Serial.println("manifest.json not found on SD card");
    return false;
  }

  String content = f.readString();
  f.close();

  int count = 0;
  int searchFrom = 0;

  while (true) {
    int q1 = content.indexOf('"', searchFrom);
    if (q1 < 0) break;
    int q2 = content.indexOf('"', q1 + 1);
    if (q2 < 0) break;

    String token = content.substring(q1 + 1, q2);
    if (token != "count" && token != "items") {
      if (count < MAX_ITEMS) manifestItems[count++] = token;
    }
    searchFrom = q2 + 1;
  }

  manifestCount = count;
  Serial.printf("manifest.json loaded: %d pairs\n", manifestCount);
  return manifestCount > 0;
}

// ====================================================================
// RANDOM PICKER
// ====================================================================

String pickRandomBasename(const String &avoid) {
  if (manifestCount == 0) return String("");
  if (manifestCount == 1) return manifestItems[0];

  String pick;
  int guard = 0;
  do {
    int idx = random(0, manifestCount);
    pick = manifestItems[idx];
    guard++;
  } while (pick == avoid && guard < 20);

  return pick;
}
