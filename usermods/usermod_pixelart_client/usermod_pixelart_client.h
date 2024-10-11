#pragma once

#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h> // Include the Wi-Fi library
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#else
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#endif
#include <WiFiUdp.h>
#include <FastLED.h>
#include <wled.h>
#include <FX.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Library inclusions.
/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 *
 * This is an example for a v2 usermod.
 * v2 usermods are class inheritance based and can (but don't have to) implement more functions, each of them is shown in this example.
 * Multiple v2 usermods can be added to one compilation easily.
 *
 * Creating a usermod:
 * This file serves as an example. If you want to create a usermod, it is recommended to use usermod_v2_empty.h from the usermods folder as a template.
 * Please remember to rename the class and file to a descriptive name.
 * You may also use multiple .h and .cpp files.
 *
 * Using a usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

/// Representation of an RGBA pixel (Red, Green, Blue, Alpha)
struct CRGBA
{
	union
	{
		struct
		{
			union
			{
				uint8_t r;
				uint8_t red;
			};
			union
			{
				uint8_t g;
				uint8_t green;
			};
			union
			{
				uint8_t b;
				uint8_t blue;
			};
			union
			{
				uint8_t a;
				uint8_t alpha;
			};
		};
		uint8_t raw[4];
	};

	/// allow copy construction
	inline CRGBA(const CRGBA &rhs) __attribute__((always_inline)) = default;

	/// allow assignment from one RGB struct to another
	inline CRGBA &operator=(const CRGBA &rhs) __attribute__((always_inline)) = default;

	// default values are UNINITIALIZED
	inline CRGBA() __attribute__((always_inline)) = default;

	/// allow construction from R, G, B, A
	inline CRGBA(uint8_t ir, uint8_t ig, uint8_t ib, uint8_t ia) __attribute__((always_inline))
		: r(ir), g(ig), b(ib), a(ia)
	{
	}

	/// allow assignment from R, G, and B
	inline CRGBA &setRGB(uint8_t nr, uint8_t ng, uint8_t nb) __attribute__((always_inline))
	{
		r = nr;
		g = ng;
		b = nb;
		return *this;
	}

	CRGB toCRGB()
	{
		return CRGB(this->red, this->green, this->blue);
	}
};

CRGBA &nblend_a(CRGBA &existing, const CRGBA &overlay, fract8 amountOfOverlay)
{
	if (amountOfOverlay == 0)
	{
		return existing;
	}

	if (amountOfOverlay == 255)
	{
		existing = overlay;
		return existing;
	}

	// Corrected blend method, with no loss-of-precision rounding errors
	existing.red = blend8(existing.red, overlay.red, amountOfOverlay);
	existing.green = blend8(existing.green, overlay.green, amountOfOverlay);
	existing.blue = blend8(existing.blue, overlay.blue, amountOfOverlay);
	existing.alpha = blend8(existing.alpha, overlay.alpha, amountOfOverlay);

	return existing;
}

CRGBA blend_a(const CRGBA &p1, const CRGBA &p2, fract8 amountOfP2)
{
	CRGBA nu(p1);
	nblend_a(nu, p2, amountOfP2);
	return nu;
}

CRGB flatten(CRGBA &overlay, CRGB &existing)
{
	int const amountOfOverlay = overlay.alpha;
	if (amountOfOverlay == 0)
	{
		return existing;
	}

	if (amountOfOverlay == 255)
	{
		return overlay.toCRGB();
	}
	// Corrected blend method, with no loss-of-precision rounding errors
	CRGB rCRGB = CRGB(blend8(existing.red, overlay.red, amountOfOverlay), blend8(existing.green, overlay.green, amountOfOverlay), blend8(existing.blue, overlay.blue, amountOfOverlay));

	return rCRGB;
}

// class name. Use something descriptive and leave the ": public Usermod" part :)
class PixelArtClient : public Usermod
{

private:
	// Private class members. You can declare variables and functions only accessible to your usermod here
	bool enabled = false;
	bool initDone = false;
	unsigned long refreshTime = 0;
	unsigned long lastRequestTime = 0;

	// set your config variables to their boot default value (this can also be done in readFromConfig() or a constructor if you prefer)
	String serverName = "https://app.pixelart-exchange.au/";
	String apiKey = "your_api_key";
	String clientName = "WLED";
	bool transparency = false;
	bool serverUp = false;
	long unsigned int serverTestRepeatTime = 10;

	// These config variables have defaults set inside readFromConfig()
	int testInt;
	long testLong;
	int8_t testPins[2];
	int crossfadeIncrement = 10;
	int crossfadeFrameRate = 40;
	std::vector<std::vector<std::vector<CRGBA>>> image1;
	std::vector<std::vector<std::vector<CRGBA>>> image2;
	std::vector<int> image1durations;
	std::vector<int> image2durations;

	std::vector<std::vector<std::vector<CRGBA>>> *currentImage;
	std::vector<std::vector<std::vector<CRGBA>>> *nextImage;
	std::vector<int> *nextImageDurations;
	std::vector<int> *currentImageDurations;

	// need a struct to hold all this (pixels, frameCount, frame timings )
	int nextImageFrameCount = 1;
	int currentImageFrameCount = 1;
	CRGB nextImageBackgroundColour;
	CRGB currentImageBackgroundColour;
	int numFrames;

	int currentFrameIndex = 0;
	// within the image, we may have one or more frames
	std::vector<std::vector<CRGBA>> currentFrame;
	unsigned int currentFrameDuration;
	// within the next image,cache the first frame for a transition
	std::vector<std::vector<CRGBA>> nextFrame;
	int nextBlend = 0;

	// time betwwen images
	int duration;
	// in seconds
	unsigned int imageDuration = 10; // Warning !!!
	bool imageLoaded = false;
	int imageIndex = 0;

	HTTPClient http;
	WiFiClient client;

	String playlist;
	String name;

	// string that are used multiple time (this will save some flash memory)
	static const char _name[];
	static const char _enabled[];

	static PixelArtClient *instance; // Singleton

	unsigned long lastTrigger = 0;
	const unsigned int triggerDelay = 200;

	inline bool isEffectActive()
	{
		return millis() < lastTrigger + triggerDelay;
	}

public:
	// non WLED related methods, may be used for data exchange between usermods (non-inline methods should be defined out of class)

	/**
	 * Enable/Disable the usermod
	 */
	inline void enable(bool enable) { enabled = enable; }

	/**
	 * Get usermod enabled/disabled state
	 */
	inline bool isEnabled() { return enabled; }

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static PixelArtClient *getInstance()
	{
		// If the instance doesn't exist, create it
		if (instance == nullptr)
		{
			instance = new PixelArtClient();
		}
		return instance;
	}

	void triggerAlive()
	{
		lastTrigger = millis();
	}

	static uint16_t dummyEffect()
	{
		return FRAMETIME;
	}

	static uint16_t mode_pixelart(void)
	{
		if (instance != nullptr)
		{
			instance->triggerAlive();
		}
		
		return PixelArtClient::dummyEffect();
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void requestImageFrames()
	{
		// Your Domain name with URL path or IP address with path

		currentImage = imageIndex ? &image1 : &image2;
		nextImage = imageIndex ? &image2 : &image1;
		currentImageDurations = imageIndex ? &image1durations : &image2durations;
		nextImageDurations = imageIndex ? &image2durations : &image1durations;
		const String serverPath = "api/image/pixels";
		const String clientPhrase = "screen_id=" + clientName;
		const String keyPhrase = "&key=" + apiKey;

		const String width = String(strip._segments[strip.getCurrSegmentId()].maxWidth);
		const String height = String(strip._segments[strip.getCurrSegmentId()].maxHeight);
		const String getUrl = serverName + (serverName.endsWith("/") ? "" : "/") + serverPath + "?" + clientPhrase + keyPhrase + "&width=" + width + "&height=" + height;

		http.begin(client, (getUrl).c_str());

		// Send HTTP GET request, turn back to http 1.0 for streaming
		http.useHTTP10(true);
		int httpResponseCode = http.GET();
		if (httpResponseCode != 200)
		{
			http.end();
			return;
		}
		
		DynamicJsonDocument doc(2048);
		Stream &client = http.getStream();

		client.find("\"meta\"");
		client.find(":");
		// meta
		DeserializationError error = deserializeJson(doc, client);

		if (error)
		{
			imageLoaded = false;
			return;
		}

		const unsigned int totalFrames = doc["frames"];
		nextImageBackgroundColour = hexToCRGB(doc["backgroundColor"]);
		const unsigned int returnHeight = doc["height"];
		const unsigned int returnWidth = doc["width"];
		imageDuration = doc["duration"];
		const char *path = doc["path"]; // "ms-pacman.gif"
		name = String(path);

		(*nextImage).resize(totalFrames);

		for (size_t i = 0; i < totalFrames; i++)
		{
			(*nextImage)[i].resize(returnHeight);
			for (size_t j = 0; j < returnHeight; j++)
			{
				(*nextImage)[i][j].resize(returnWidth);
			}
		}

		nextImageDurations->resize(totalFrames);

		client.find("\"rows\"");
		client.find("[");
		do
		{
			deserializeJson(doc, client);
			
			// read row metadata
			int frame_duration = doc["duration"]; // 200, 200, 200
			int frameIndex = doc["frame"];		  // 200, 200, 200
			(*nextImageDurations)[frameIndex] = frame_duration;
			int rowIndex = doc["row"]; // 200, 200, 200

			JsonArray rowPixels = doc["pixels"].as<JsonArray>();
			int colIndex = 0;

			(*nextImage)[frameIndex][rowIndex].resize(returnWidth);

			for (JsonVariant pixel : rowPixels)
			{
				const char *pixelStr = (pixel.as<const char *>());
				const CRGBA color = hexToCRGBA(String(pixelStr));
				(*nextImage)[frameIndex][rowIndex][colIndex] = color;
				colIndex++;
			}

		} while (client.findUntil(",", "]"));

		// Free resources
		http.end();

		if (error)
		{
			imageLoaded = false;
			return;
		}

		nextImageFrameCount = totalFrames;

		imageLoaded = true;
	}

	CRGB hexToCRGB(String hexString)
	{
		// Convert the hex string to an integer value
		uint32_t hexValue = strtoul(hexString.c_str(), NULL, 16);

		// Extract the red, green, and blue components from the hex value
		uint8_t red = (hexValue >> 16) & 0xFF;
		uint8_t green = (hexValue >> 8) & 0xFF;
		uint8_t blue = hexValue & 0xFF;

		// Create a CRGB object with the extracted components
		CRGB color = CRGB(red, green, blue);

		return color;
	}
	CRGBA hexToCRGBA(String hexString)
	{
		// Convert the hex string to an integer value
		uint32_t hexValue = strtoul(hexString.c_str(), NULL, 16);

		// Extract the red, green, and blue components from the hex value
		uint8_t red = (hexValue >> 24) & 0xFF;
		uint8_t green = (hexValue >> 16) & 0xFF;
		uint8_t blue = (hexValue >> 8) & 0xFF;
		uint8_t alpha = hexValue & 0xFF;

		// Create a CRGB object with the extracted components
		CRGBA color = CRGBA(red, green, blue, alpha);

		return color;
	}

	void completeImageTransition()
	{

		// flip to the next image for the next request
		imageIndex = imageIndex == 0 ? 1 : 0;
		// and flip which image is which, so nextImage -> currentImage
		// used in next redraw
		currentImage = imageIndex ? &image1 : &image2;
		currentImageDurations = imageIndex ? &image1durations : &image2durations;

		// reuse this for the next image load
		nextImage = imageIndex ? &image2 : &image1;
		nextImageDurations = imageIndex ? &image2durations : &image1durations;
		currentImageFrameCount = nextImageFrameCount;
		currentImageBackgroundColour = nextImageBackgroundColour;

		currentFrameIndex = 0;
		currentFrame = (*currentImage)[currentFrameIndex];
		currentFrameDuration = (*currentImageDurations)[currentFrameIndex];
	}

	void getImage()
	{
		// Send request
		requestImageFrames();

		// prime these for next redraw
		if (imageLoaded)
		{
			if (image1.size() > 0 && image2.size() > 0)
			{
				// we have 2 images, crossfade them
				nextBlend = crossfadeIncrement;
				nextFrame = (*nextImage)[0];
			}
			else
			{
				// first load? just show it;
				completeImageTransition();
			}
		}
	}

	// methods called by WLED (can be inlined as they are called only once but if you call them explicitly define them out of class)

	/*
	 * setup() is called once at boot. WiFi is not yet connected at this point.
	 * readFromConfig() is called prior to setup()
	 * You can use it to initialize variables, sensors or similar.
	 */
	void setup()
	{
		// do your set-up here
		Serial.print("Hello from pixel art grabber! WLED matrix mode on? ");
		Serial.println(strip.isMatrix);
		initDone = true;
		strip.addEffect(255, &PixelArtClient::mode_pixelart, "Pixel Art@Transition Speed;;;2");
	}

	void checkin()
	{
		const String width = String(strip._segments[strip.getCurrSegmentId()].maxWidth);
		const String height = String(strip._segments[strip.getCurrSegmentId()].maxHeight);
		const String getUrl = serverName + (serverName.endsWith("/") ? "api/client/checkin?id=" : "/api/client/checkin?id=") + clientName + "&width=" + width + "&height=" + height;
		//Serial.println(getUrl);
		http.begin(client, (getUrl).c_str());

		// Send HTTP GET request
		int httpResponseCode = http.GET();
		serverUp = (httpResponseCode == 200);
		http.end();
	}

	/*
	 * connected() is called every time the WiFi is (re)connected
	 * Use it to initialize network interfaces
	 */
	void connected()
	{
		Serial.println("Connected to WiFi, checking in!");
		checkin();
	}

	/*
	 * loop() is called continuously. Here you can check for events, read sensors, etc.
	 *
	 * Tips:
	 * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
	 *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
	 *
	 * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
	 *    Instead, use a timer check as shown here.
	 */
	void loop()
	{
		// if usermod is disabled or called during strip updating just exit
		// NOTE: on very long strips strip.isUpdating() may always return true so update accordingly

		if (!enabled || !strip.isMatrix || !isEffectActive())
			return;

		if (!serverUp && ((millis() - lastRequestTime) > serverTestRepeatTime * 1000))
		{
			lastRequestTime = millis();
			checkin();
			return;
		}

		// request next image
		if (millis() - lastRequestTime > imageDuration * 1000)
		{
			lastRequestTime = millis();
			getImage();
		}
	}

	void setPixelsFrom2DVector(const std::vector<std::vector<CRGBA>> &pixelValues, CRGB backgroundColour)
	{

		// iterate through the 2D vector of CRGBA values
		int whichRow = 0;
		for (std::vector<CRGBA> row : pixelValues)
		{
			int whichCol = 0;
			for (CRGBA pixel : row)
			{
				CRGB finalColour;
				if (transparency)
				{
					CRGB existing = strip.getPixelColorXY(whichCol, whichRow);
					finalColour = flatten(pixel, existing);
				}
				else
				{
					finalColour = flatten(pixel, backgroundColour);
				}
				strip.setPixelColorXY(whichCol, whichRow, finalColour);
				whichCol++;
			}
			whichRow++;
		}
	}

	void setPixelsFrom2DVector(std::vector<std::vector<CRGBA>> &currentPixels, std::vector<std::vector<CRGBA>> &nextPixels, int &blendPercent, CRGB &backgroundColour)
	{

		// iterate through the 2D vector of CRGB values
		int whichRow = 0;
		for (std::vector<CRGBA> row : currentPixels)
		{
			int whichCol = 0;
			for (CRGBA pixel : row)
			{
				const CRGBA targetPixel = nextFrame[whichRow][whichCol];
				CRGBA newPixel = blend_a(pixel, targetPixel, blendPercent);
				CRGB finalColour;
				if (transparency)
				{
					CRGB existing = strip.getPixelColorXY(whichCol, whichRow);
					finalColour = flatten(newPixel, existing);
				}
				else
				{
					finalColour = flatten(newPixel, backgroundColour);
				}
				strip.setPixelColorXY(whichCol, whichRow, finalColour);
				whichCol++;
			}
			whichRow++;
		}
	}

	/*
	 * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
	 * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
	 * Below it is shown how this could be used for e.g. a light sensor
	 */
	void addToJsonInfo(JsonObject &root)
	{
		// if "u" object does not exist yet wee need to create it
		JsonObject user = root["u"];
		if (user.isNull())
			user = root.createNestedObject("u");
	}

	/*
	 * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
	 * Values in the state object may be modified by connected clients
	 */
	void addToJsonState(JsonObject &root)
	{
		if (!initDone || !enabled)
			return; // prevent crash on boot applyPreset()

		JsonObject usermod = root[FPSTR(_name)];
		if (usermod.isNull())
			usermod = root.createNestedObject(FPSTR(_name));
	}

	/*
	 * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
	 * Values in the state object may be modified by connected clients
	 */
	void readFromJsonState(JsonObject &root)
	{
		if (!initDone)
			return; // prevent crash on boot applyPreset()

		JsonObject usermod = root[FPSTR(_name)];
		if (!usermod.isNull())
		{
			// expect JSON usermod data in usermod name object: {"ExampleUsermod:{"user0":10}"}
			userVar0 = usermod["user0"] | userVar0; // if "user0" key exists in JSON, update, else keep old value
		}
		// you can as well check WLED state JSON keys
		// if (root["bri"] == 255) Serial.println(F("Don't burn down your garage!"));
	}

	/*
	 * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
	 * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
	 * If you want to force saving the current state, use serializeConfig() in your loop().
	 */
	void addToConfig(JsonObject &root)
	{
		JsonObject top = root.createNestedObject(FPSTR(_name));
		top[FPSTR(_enabled)] = enabled;
		// save these vars persistently whenever settings are saved
		top["server url"] = serverName;
		top["api key"] = apiKey;
		top["screen id"] = clientName;
		top["transparent"] = transparency;
	}

	/*
	 * readFromConfig() can be used to read back the custom settings you added with addToConfig().
	 * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
	 */
	bool readFromConfig(JsonObject &root)
	{
		JsonObject top = root[FPSTR(_name)];

		bool configComplete = !top.isNull();

		configComplete &= getJsonValue(top["enabled"], enabled);
		configComplete &= getJsonValue(top["server url"], serverName);
		configComplete &= getJsonValue(top["screen id"], clientName);
		configComplete &= getJsonValue(top["api key"], apiKey);
		configComplete &= getJsonValue(top["transparent"], transparency);
		return configComplete;
	}

	/*
	 * appendConfigData() is called when user enters usermod settings page
	 * it may add additional metadata for certain entry fields (adding drop down is possible)
	 * be careful not to add too much as oappend() buffer is limited to 3k
	 */
	void appendConfigData()
	{
		oappend(SET_F("addInfo('PixelArtClient:server url', 1, '');"));
		oappend(SET_F("addInfo('PixelArtClient:screen id', 1, '');"));
		oappend(SET_F("addInfo('PixelArtClient:api key', 1, '');"));
		oappend(SET_F("addField('PixelArtClient:transparent', 1, true);"));
	}

	/*
	 * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
	 * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
	 * Commonly used for custom clocks (Cronixie, 7 segment)
	 */
	void handleOverlayDraw()
	{
		// draw currently cached image again
		if (enabled && imageLoaded && isEffectActive())
		{
			// cycle frames within a multi-frame image (ie animated gif)
			if (millis() - refreshTime > currentFrameDuration)
			{
				refreshTime = millis();
				currentFrameIndex++;
				currentFrameIndex = currentFrameIndex % currentImageFrameCount;
				// choose next frame in set to update
				currentFrame = (*currentImage)[currentFrameIndex];
				currentFrameDuration = (*currentImageDurations)[currentFrameIndex];
			}

			if (nextBlend > 0)
			{
				setPixelsFrom2DVector(currentFrame, nextFrame, nextBlend, currentImageBackgroundColour);
				nextBlend += crossfadeIncrement;
				
				if (nextBlend > 255)
				{
					nextBlend = 0;
					completeImageTransition();
				}
			}
			else
			{
				setPixelsFrom2DVector(currentFrame, currentImageBackgroundColour);
			}
		}
	}

	/*
	 * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
	 * This could be used in the future for the system to determine whether your usermod is installed.
	 */
	uint16_t getId()
	{
		return USERMOD_ID_PIXELART_CLIENT;
	}

	// More methods can be added in the future, this example will then be extended.
	// Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};

PixelArtClient *PixelArtClient::instance = nullptr;

// add more strings here to reduce flash memory usage
const char PixelArtClient::_name[] PROGMEM = "PixelArtClient";
const char PixelArtClient::_enabled[] PROGMEM = "enabled";

