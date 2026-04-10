#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "SPI.h"
#include "TFT_eSPI.h"
#include "Encoder.h"
#include <vector>

// --- PIN MAPPING (Based on your final schematic image_8e8562.png) ---
#define SD_CS          27 
#define I2S_DOUT       24 
#define I2S_BCLK       25 
#define I2S_LRC        23 
#define ENC_A          11 
#define ENC_B          14 
#define ENC_SW         28 
#define BTN_UP         21 // s1
#define BTN_DOWN       22 // s2
#define BTN_MENU       6  // s3
#define BTN_PWR        26 // s4

// --- UI THEME (dB-1 Minimalist) ---
#define BG_COLOR    0x0000 // Black
#define ACCENT      0x780F // Deep Purple
#define TEXT_COLOR  0xFFFF // White
#define GREY        0x7BEF

// --- OBJECTS ---
Audio audio;
TFT_eSPI tft = TFT_eSPI();
Encoder rotary(ENC_A, ENC_B);

// --- STATE VARIABLES ---
int volume = 12;
bool inMenu = true;
int menuIdx = 0;
std::vector<String> songs;
unsigned long pwrTimer = 0;
bool isRunning = true;
String currentSong = "None";

void setup() {
    Serial.begin(115200);
    
    // Pin Modes with Internal Pullups
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_MENU, INPUT_PULLUP);
    pinMode(BTN_PWR, INPUT_PULLUP);
    pinMode(ENC_SW, INPUT_PULLUP);

    // Display Init
    tft.init();
    tft.setRotation(1);
    drawSplash();

    // SD Init
    if(!SD.begin(SD_CS)) {
        tft.fillScreen(TFT_RED);
        tft.drawString("SD ERROR", 80, 100, 4);
        while(1);
    }

    scanSD();
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(volume);
    
    drawUI();
}

void loop() {
    if(!isRunning) return;

    audio.loop();
    handleNavigation();
    handlePower();
}

// --- CORE LOGIC ---

void scanSD() {
    File root = SD.open("/");
    while (File file = root.openNextFile()) {
        String name = file.name();
        if (name.endsWith(".mp3")) {
            songs.push_back("/" + name);
        }
        file.close();
    }
}

void handleNavigation() {
    // 1. Rotary Encoder Logic
    static long oldPos = 0;
    long newPos = rotary.read() / 4;
    
    if (newPos != oldPos) {
        if (inMenu) {
            menuIdx = constrain(menuIdx + (newPos - oldPos), 0, (int)songs.size() - 1);
            drawMenu();
        } else {
            volume = constrain(volume + (newPos - oldPos), 0, 21);
            audio.setVolume(volume);
            drawPlayerOverlay(); // Update volume bar only
        }
        oldPos = newPos;
    }

    // 2. Button Logic
    if (digitalRead(BTN_MENU) == LOW) { // Toggle View
        delay(200);
        inMenu = !inMenu;
        drawUI();
    }

    if (inMenu && (digitalRead(ENC_SW) == LOW)) { // Select & Play
        delay(200);
        currentSong = songs[menuIdx];
        audio.connecttoFS(SD, currentSong.c_str());
        inMenu = false;
        drawUI();
    }
}

void handlePower() {
    if (digitalRead(BTN_PWR) == LOW) {
        if (pwrTimer == 0) pwrTimer = millis();
        if (millis() - pwrTimer > 2000) { // 2 Sec Hold
            isRunning = false;
            tft.fillScreen(BG_COLOR);
            tft.setTextColor(ACCENT);
            tft.drawCentreString("GOODBYE", 160, 100, 4);
            delay(1000);
            tft.writecommand(0x10); // Display Sleep
            ESP.deepSleep(0); 
        }
    } else {
        pwrTimer = 0;
    }
}

// --- UI RENDERING ---

void drawSplash() {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(ACCENT);
    tft.drawCentreString("dB-1", 160, 80, 7);
    tft.setTextColor(GREY);
    tft.drawCentreString("SYSTEM START", 160, 160, 2);
    delay(2000);
}

void drawUI() {
    tft.fillScreen(BG_COLOR);
    if (inMenu) drawMenu();
    else drawPlayer();
}

void drawMenu() {
    tft.fillRect