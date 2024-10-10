#pragma once

#include "wled.h"

/**
 * Clock panel design :
 * 
 *           o o     o o         o o     o o
 *          o o o   o o o   o   o o o   o o o
 *           o o     o o         o o     o o
 *          o o o   o o o   o   o o o   o o o
 *           o o     o o         o o     o o
 *          \___/   \___/  \_/  \___/   \___/
 * Digit# :   1       2           3       4
 */

class ChristuxPingPongClockUsermod : public Usermod {

  private:

    // Private class members. You can declare variables and functions only accessible to your usermod here
    bool enabled = false;
    bool initDone = false;
    unsigned long lastTime = 0;
    bool colonOn = true;

    int baseH = 6;  // Address for the one place of the hours
    int baseHH = 0;  // Address for the tens place of the hours
    int baseM = 20; // Address for the one place of the minutes
    int baseMM = 14; // Address for the tens place of the minutes
    int colon1 = 37; // Address for the first colon led
    int colon2 = 87; // Address for the second colon led

    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _enabled[];

    const bool* _numbers[10] = {
			zero,
      one,
			two,
			three,
			four,
			five,
			six,
			seven,
			eight,
			nine
		};

    static const bool zero[12];
    static const bool one[12];
		static const bool two[12];
		static const bool three[12];
		static const bool four[12];
		static const bool five[12];
		static const bool six[12];
		static const bool seven[12];
		static const bool eight[12];
		static const bool nine[12];

    static const uint32_t blank = RGBW32(0,0,0,0);

    int getLedPosition(int i, int j);
    void displayDigit(int position, int n);

  public:

    /**
     * Enable/Disable the usermod
     */
    inline void enable(bool enable) { enabled = enable; }

    /**
     * Get usermod enabled/disabled state
     */
    inline bool isEnabled() { return enabled; }

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * readFromConfig() is called prior to setup()
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() {
      // do your set-up here
      Serial.println("Hello from Christux Ping Pong Clock usermod!");
      initDone = true;
    }

    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     */
    void loop() {
      // if usermod is disabled or called during strip updating just exit
      // NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
      if (!enabled || strip.isUpdating()) return;

      // do your magic here
      if (millis() - lastTime > 1000) {
        lastTime = millis();
        colonOn = !colonOn;
      }
    }

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root)
    {
      // if "u" object does not exist yet wee need to create it
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
    }

    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
    }

    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     */
    bool readFromConfig(JsonObject& root)
    {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top["enabled"], enabled);
      return configComplete;
    }

    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw()
    {
      if (enabled) 
      {
        if(colonOn)
        {
          strip.setPixelColor(colon1, blank);
          strip.setPixelColor(colon2, blank);
        }

        displayDigit(baseHH, (hour(localTime) / 10) % 10);
        displayDigit(baseH, hour(localTime) % 10); 
        displayDigit(baseMM, (minute(localTime) / 10) % 10);
        displayDigit(baseM, minute(localTime) % 10);
      }
    }

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_CHRISTUX_PING_PONG_CLOCK;
    }
};

// add more strings here to reduce flash memory usage
const char ChristuxPingPongClockUsermod::_name[]    PROGMEM = "ChristuxPingPongClock";
const char ChristuxPingPongClockUsermod::_enabled[] PROGMEM = "enabled";

// implementation of non-inline member methods

int ChristuxPingPongClockUsermod::getLedPosition(int i, int j) 
{
  return i * 25 + j;
}

void ChristuxPingPongClockUsermod::displayDigit(int position, int digit)
{
  for(int i = 0, pos = 0; i < 5; i++) {
    for (int j = 0, k = i & 0x01; j < 2 + k; j++) {
      if(!_numbers[digit][pos]) {
        strip.setPixelColor(i * 25 + 2 * j + 1 - k + position, blank);
      }
      pos++;
    }
  }
}

const bool ChristuxPingPongClockUsermod::zero[] = {
      1,1,
    1,0,1,
      0,0,
    1,0,1,
      1,1
};

const bool ChristuxPingPongClockUsermod::one[] = {
      0,1,
    0,0,1,
      0,1,
    0,0,1,
      0,1
};

const bool ChristuxPingPongClockUsermod::two[] = {
      1,1,
    0,0,1,
      1,1,
    1,0,0,
      1,1
};

const bool ChristuxPingPongClockUsermod::three[] = {
      1,1,
    0,0,1,
      1,1,
    0,0,1,
      1,1
};

const bool ChristuxPingPongClockUsermod::four[] = {
      1,0,
    1,0,1,
      1,1,
    0,0,1,
      0,1
};

const bool ChristuxPingPongClockUsermod::five[] = {
      1,1,
    1,0,0,
      1,1,
    0,0,1,
      1,1
};

const bool ChristuxPingPongClockUsermod::six[] = {
      0,1,
    0,1,0,
      1,1,
    1,0,1,
      1,1
};

const bool ChristuxPingPongClockUsermod::seven[] = {
      1,1,
    0,0,1,
      0,1,
    0,1,0,
      1,0
};

const bool ChristuxPingPongClockUsermod::eight[] = {
      1,1,
    1,0,1,
      1,1,
    1,0,1,
      1,1
};

const bool ChristuxPingPongClockUsermod::nine[] = {
      1,1,
    1,0,1,
      1,1,
    0,1,0,
      1,0
};
