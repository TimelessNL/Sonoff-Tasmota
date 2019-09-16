/*
  xplg_ws2812.ino - ws2812 led string support for Sonoff-Tasmota

  Copyright (C) 2019  Heiko Krupp and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_LIGHT
#ifdef USE_WS2812
/*********************************************************************************************\
 * WS2812 RGB / RGBW Leds using NeopixelBus library
\*********************************************************************************************/

#include <NeoPixelBus.h>

#if (USE_WS2812_CTYPE == NEO_GRB)
  typedef NeoGrbFeature selectedNeoFeatureType;
#elif (USE_WS2812_CTYPE == NEO_BRG)
  typedef NeoBrgFeature selectedNeoFeatureType;
#elif (USE_WS2812_CTYPE == NEO_RBG)
  typedef NeoRbgFeature selectedNeoFeatureType;
#elif (USE_WS2812_CTYPE == NEO_RGBW)
  typedef NeoRgbwFeature selectedNeoFeatureType;
#elif (USE_WS2812_CTYPE == NEO_GRBW)
  typedef NeoGrbwFeature selectedNeoFeatureType;
#else   // USE_WS2812_CTYPE
  typedef NeoRgbFeature selectedNeoFeatureType;
#endif  // USE_WS2812_CTYPE


#ifdef USE_WS2812_DMA
  typedef Neo800KbpsMethod selectedNeoSpeedType;
#else   // USE_WS2812_DMA
  typedef NeoEsp8266BitBang800KbpsMethod selectedNeoSpeedType;
#endif  // USE_WS2812_DMA
  NeoPixelBus<selectedNeoFeatureType, selectedNeoSpeedType> *strip = nullptr;

struct WsColor {
  uint8_t red, green, blue;
};

struct ColorScheme {
  WsColor* colors;
  uint8_t count;
};

WsColor kIncandescent[2] = { 255,140,20, 0,0,0 };
WsColor kRgb[3] = { 255,0,0, 0,255,0, 0,0,255 };
WsColor kChristmas[2] = { 255,0,0, 0,255,0 };
WsColor kHanukkah[2] = { 0,0,255, 255,255,255 };
WsColor kwanzaa[3] = { 255,0,0, 0,0,0, 0,255,0 };
WsColor kRainbow[7] = { 255,0,0, 255,128,0, 255,255,0, 0,255,0, 0,0,255, 128,0,255, 255,0,255 };
WsColor kFire[3] = { 255,0,0, 255,102,0, 255,192,0 };
ColorScheme kSchemes[WS2812_SCHEMES] = {
  kIncandescent, 2,
  kRgb, 3,
  kChristmas, 2,
  kHanukkah, 2,
  kwanzaa, 3,
  kRainbow, 7,
  kFire, 3 };

uint8_t kWidth[5] = {
    1,     // Small
    2,     // Medium
    4,     // Large
    8,     // Largest
  255 };   // All
uint8_t kWsRepeat[5] = {
    8,     // Small
    6,     // Medium
    4,     // Large
    2,     // Largest
    1 };   // All

uint8_t ws_show_next = 1;
bool ws_suspend_update = false;
/********************************************************************************************/

void Ws2812StripShow(void)
{
#if (USE_WS2812_CTYPE > NEO_3LED)
  RgbwColor c;
#else
  RgbColor c;
#endif

  if (Settings.light_correction) {
    for (uint32_t i = 0; i < Settings.light_pixels; i++) {
      c = strip->GetPixelColor(i);
      c.R = ledGamma(c.R);
      c.G = ledGamma(c.G);
      c.B = ledGamma(c.B);
#if (USE_WS2812_CTYPE > NEO_3LED)
      c.W = ledGamma(c.W);
#endif
      strip->SetPixelColor(i, c);
    }
  }
  strip->Show();
}

int mod(int a, int b)
{
   int ret = a % b;
   if (ret < 0) ret += b;
   return ret;
}


void Ws2812UpdatePixelColor(int position, struct WsColor hand_color, float offset)
{
#if (USE_WS2812_CTYPE > NEO_3LED)
  RgbwColor color;
#else
  RgbColor color;
#endif

  uint16_t mod_position = mod(position, (int)Settings.light_pixels);

  color = strip->GetPixelColor(mod_position);
  float dimmer = 100 / (float)Settings.light_dimmer;
  color.R = tmin(color.R + ((hand_color.red / dimmer) * offset), 255);
  color.G = tmin(color.G + ((hand_color.green / dimmer) * offset), 255);
  color.B = tmin(color.B + ((hand_color.blue / dimmer) * offset), 255);
  strip->SetPixelColor(mod_position, color);
}

void Ws2812UpdateHand(int position, uint8_t index)
{
  position = (position + Settings.light_rotation) % Settings.light_pixels;

  if (Settings.flag.ws_clock_reverse) position = Settings.light_pixels -position;
  WsColor hand_color = { Settings.ws_color[index][WS_RED], Settings.ws_color[index][WS_GREEN], Settings.ws_color[index][WS_BLUE] };

  Ws2812UpdatePixelColor(position, hand_color, 1);

  uint8_t range = 1;
  if (index < WS_MARKER) range = ((Settings.ws_width[index] -1) / 2) +1;
  for (uint32_t h = 1; h < range; h++) {
    float offset = (float)(range - h) / (float)range;
    Ws2812UpdatePixelColor(position -h, hand_color, offset);
    Ws2812UpdatePixelColor(position +h, hand_color, offset);
  }
}

void Ws2812Clock(void)
{
  strip->ClearTo(0); // Reset strip
  int clksize = 60000 / (int)Settings.light_pixels;

  Ws2812UpdateHand((RtcTime.second * 1000) / clksize, WS_SECOND);
  Ws2812UpdateHand((RtcTime.minute * 1000) / clksize, WS_MINUTE);
  Ws2812UpdateHand((((RtcTime.hour % 12) * 5000) + ((RtcTime.minute * 1000) / 12 )) / clksize, WS_HOUR);
  if (Settings.ws_color[WS_MARKER][WS_RED] + Settings.ws_color[WS_MARKER][WS_GREEN] + Settings.ws_color[WS_MARKER][WS_BLUE]) {
    for (uint32_t i = 0; i < 12; i++) {
      Ws2812UpdateHand((i * 5000) / clksize, WS_MARKER);
    }
  }

  Ws2812StripShow();
}

void Ws2812GradientColor(uint8_t schemenr, struct WsColor* mColor, uint16_t range, uint16_t gradRange, uint16_t i)
{
/*
 * Compute the color of a pixel at position i using a gradient of the color scheme.
 * This function is used internally by the gradient function.
 */
  ColorScheme scheme = kSchemes[schemenr];
  uint16_t curRange = i / range;
  uint16_t rangeIndex = i % range;
  uint16_t colorIndex = rangeIndex / gradRange;
  uint16_t start = colorIndex;
  uint16_t end = colorIndex +1;
  if (curRange % 2 != 0) {
    start = (scheme.count -1) - start;
    end = (scheme.count -1) - end;
  }
  float dimmer = 100 / (float)Settings.light_dimmer;
  float fmyRed = (float)map(rangeIndex % gradRange, 0, gradRange, scheme.colors[start].red, scheme.colors[end].red) / dimmer;
  float fmyGrn = (float)map(rangeIndex % gradRange, 0, gradRange, scheme.colors[start].green, scheme.colors[end].green) / dimmer;
  float fmyBlu = (float)map(rangeIndex % gradRange, 0, gradRange, scheme.colors[start].blue, scheme.colors[end].blue) / dimmer;
  mColor->red = (uint8_t)fmyRed;
  mColor->green = (uint8_t)fmyGrn;
  mColor->blue = (uint8_t)fmyBlu;
}

void Ws2812Gradient(uint8_t schemenr)
{
/*
 * This routine courtesy Tony DiCola (Adafruit)
 * Display a gradient of colors for the current color scheme.
 *  Repeat is the number of repetitions of the gradient (pick a multiple of 2 for smooth looping of the gradient).
 */
#if (USE_WS2812_CTYPE > NEO_3LED)
  RgbwColor c;
  c.W = 0;
#else
  RgbColor c;
#endif

  ColorScheme scheme = kSchemes[schemenr];
  if (scheme.count < 2) return;

  uint8_t repeat = kWsRepeat[Settings.light_width];  // number of scheme.count per ledcount
  uint16_t range = (uint16_t)ceil((float)Settings.light_pixels / (float)repeat);
  uint16_t gradRange = (uint16_t)ceil((float)range / (float)(scheme.count - 1));
  uint16_t speed = ((Settings.light_speed * 2) -1) * (STATES / 10);
  uint16_t offset = speed > 0 ? strip_timer_counter / speed : 0;

  WsColor oldColor, currentColor;
  Ws2812GradientColor(schemenr, &oldColor, range, gradRange, offset);
  currentColor = oldColor;
  for (uint32_t i = 0; i < Settings.light_pixels; i++) {
    if (kWsRepeat[Settings.light_width] > 1) {
      Ws2812GradientColor(schemenr, &currentColor, range, gradRange, i +offset);
    }
    if (Settings.light_speed > 0) {
      // Blend old and current color based on time for smooth movement.
      c.R = map(strip_timer_counter % speed, 0, speed, oldColor.red, currentColor.red);
      c.G = map(strip_timer_counter % speed, 0, speed, oldColor.green, currentColor.green);
      c.B = map(strip_timer_counter % speed, 0, speed, oldColor.blue, currentColor.blue);
    }
    else {
      // No animation, just use the current color.
      c.R = currentColor.red;
      c.G = currentColor.green;
      c.B = currentColor.blue;
    }
    strip->SetPixelColor(i, c);
    oldColor = currentColor;
  }
  Ws2812StripShow();
}

void Ws2812Bars(uint8_t schemenr)
{
/*
 * This routine courtesy Tony DiCola (Adafruit)
 * Display solid bars of color for the current color scheme.
 * Width is the width of each bar in pixels/lights.
 */
#if (USE_WS2812_CTYPE > NEO_3LED)
  RgbwColor c;
  c.W = 0;
#else
  RgbColor c;
#endif
  uint16_t i;

  ColorScheme scheme = kSchemes[schemenr];

  uint16_t maxSize = Settings.light_pixels / scheme.count;
  if (kWidth[Settings.light_width] > maxSize) maxSize = 0;

  uint16_t speed = ((Settings.light_speed * 2) -1) * (STATES / 10);
  uint8_t offset = speed > 0 ? strip_timer_counter / speed : 0;

  WsColor mcolor[scheme.count];
  memcpy(mcolor, scheme.colors, sizeof(mcolor));
  float dimmer = 100 / (float)Settings.light_dimmer;
  for (i = 0; i < scheme.count; i++) {
    float fmyRed = (float)mcolor[i].red / dimmer;
    float fmyGrn = (float)mcolor[i].green / dimmer;
    float fmyBlu = (float)mcolor[i].blue / dimmer;
    mcolor[i].red = (uint8_t)fmyRed;
    mcolor[i].green = (uint8_t)fmyGrn;
    mcolor[i].blue = (uint8_t)fmyBlu;
  }
  uint8_t colorIndex = offset % scheme.count;
  for (i = 0; i < Settings.light_pixels; i++) {
    if (maxSize) colorIndex = ((i + offset) % (scheme.count * kWidth[Settings.light_width])) / kWidth[Settings.light_width];
    c.R = mcolor[colorIndex].red;
    c.G = mcolor[colorIndex].green;
    c.B = mcolor[colorIndex].blue;
    strip->SetPixelColor(i, c);
  }
  Ws2812StripShow();
}

/*********************************************************************************************\
 * Public
\*********************************************************************************************/

void Ws2812Init(void)
{
#ifdef USE_WS2812_DMA
  strip = new NeoPixelBus<selectedNeoFeatureType, selectedNeoSpeedType>(WS2812_MAX_LEDS);  // For Esp8266, the Pin is omitted and it uses GPIO3 due to DMA hardware use.
#else  // USE_WS2812_DMA
  strip = new NeoPixelBus<selectedNeoFeatureType, selectedNeoSpeedType>(WS2812_MAX_LEDS, pin[GPIO_WS2812]);
#endif  // USE_WS2812_DMA
  strip->Begin();
  Ws2812Clear();
}

void Ws2812Clear(void)
{
  strip->ClearTo(0);
  strip->Show();
  ws_show_next = 1;
}

void Ws2812SetColor(uint16_t led, uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
{
#if (USE_WS2812_CTYPE > NEO_3LED)
  RgbwColor lcolor;
  lcolor.W = white;
#else
  RgbColor lcolor;
#endif

  lcolor.R = red;
  lcolor.G = green;
  lcolor.B = blue;
  if (led) {
    strip->SetPixelColor(led -1, lcolor);  // Led 1 is strip Led 0 -> substract offset 1
  } else {
//    strip->ClearTo(lcolor);  // Set WS2812_MAX_LEDS pixels
    for (uint32_t i = 0; i < Settings.light_pixels; i++) {
      strip->SetPixelColor(i, lcolor);
    }
  }

  if (!ws_suspend_update) {
    strip->Show();
    ws_show_next = 1;
  }
}

void Ws2812ForceSuspend (void) {
  ws_suspend_update = true;
}

void Ws2812ForceUpdate (void) {
  ws_suspend_update = false;
  strip->Show();
  ws_show_next = 1;
}

char* Ws2812GetColor(uint16_t led, char* scolor)
{
  uint8_t sl_ledcolor[4];

 #if (USE_WS2812_CTYPE > NEO_3LED)
  RgbwColor lcolor = strip->GetPixelColor(led -1);
  sl_ledcolor[3] = lcolor.W;
 #else
  RgbColor lcolor = strip->GetPixelColor(led -1);
 #endif
  sl_ledcolor[0] = lcolor.R;
  sl_ledcolor[1] = lcolor.G;
  sl_ledcolor[2] = lcolor.B;
  scolor[0] = '\0';
  for (uint32_t i = 0; i < light_subtype; i++) {
    if (Settings.flag.decimal_text) {
      snprintf_P(scolor, 25, PSTR("%s%s%d"), scolor, (i > 0) ? "," : "", sl_ledcolor[i]);
    } else {
      snprintf_P(scolor, 25, PSTR("%s%02X"), scolor, sl_ledcolor[i]);
    }
  }
  return scolor;
}

void Ws2812ShowScheme(uint8_t scheme)
{
  switch (scheme) {
    case 0:  // Clock
      if ((1 == state_250mS) || (ws_show_next)) {
        Ws2812Clock();
        ws_show_next = 0;
      }
      break;
    case 8: //World Clock
        Ws2812WorldClock();
        break;
    default:
      if (1 == Settings.light_fade) {
        Ws2812Gradient(scheme -1);
      } else {
        Ws2812Bars(scheme -1);
      }
      ws_show_next = 1;
      break;
  }
}

// World clock logic

//Actual words as array variables
int Hashtags[]           = {3, 6, 43, 71, -1};
int WordHet[]            = {0, 1, 2, -1};
int WordIs[]             = {4, 5, -1};
int WordBijna[]          = {7, 8, 9, 10, 11, -1};
int WordTwintig[]        = {12, 13, 14, 15, 16, 17, 18, -1};
int WordKwart[]          = {19, 20, 21, 22, 23, -1};
int WordMinVijf[]        = {24, 25, 26, 27, -1};
int WordMinTien[]        = {28, 29, 30, 31, -1};
int WordWeatherGaat[]    = {32, 33, 34, 35, -1};
int WordMinuten[]        = {36, 37, 38, 39, 40, 41, 42, -1};
int WordVoor[]           = {44, 45, 46, 47, -1};
int WordOver[]           = {48, 49, 50, 51, -1};
int WordTempTwee[]       = {52, 53, 54, 55, -1};
int WordHalf[]           = {56, 57, 58, 59, -1};
int WordEen[]            = {60, 61, 62, -1};
int WordTwee[]           = {63, 64, 65, 66, -1};
int WordDrie[]           = {67, 68, 69, 70, -1};
int WordVier[]           = {72, 73, 74, 75, -1};
int WordVijf[]           = {76, 77, 78, 79, -1};
int WordTempDrie[]       = {80, 81, 82, 83, -1};
int WordZeven[]          = {84, 85, 86, 87, 88, -1};
int WordAcht[]           = {89, 90, 91, 92, -1};
int WordElf[]            = {93, 94, 95, -1};
int WordNegen[]          = {96, 97, 98, 99, 100, -1};
int WordTien[]           = {101, 102, 103, 104, -1};
int WordTempEen[]        = {105, 106, 107, -1};
int WordTwaalf[]         = {108, 109, 110, 111, 112, 113, -1};
int WordZes[]            = {114, 115, 116, -1};
int WordUur[]            = {117, 118, 119, -1};
int WordTempGraden[]     = {120, 121, 122, 123, 124, 125, -1};
int WordWeatherZonnig[]  = {126, 127, 128, 129, 130, 131 -1};
int WordWeatherRegenen[] = {132, 133, 134, 135, 136, 137, 138, -1};
int WordWeatherDroog[]   = {139, 140, 141, 142, 143, -1};

void Ws2812WorldClock()
{
    byte second, minute, hour;

    second = RtcTime.second;
    minute = RtcTime.minute;
    hour   = RtcTime.hour;

    // light up "het" it stays on
    lightup(WordHet, true, 1);

    // light off "hastags" is always off
    lightup(Hashtags, false, 1);

    lightupAbout(minute);
    lightupHourMinute(hour, minute);
    Ws2812StripShow();
}

void lightupAbout(int minute){
    // If it's bang on 5 mins, or 10 mins etc, it's not 'about' so turn it off.
    if((minute == 0)
      |(minute == 5)
      |(minute == 10)
      |(minute == 15)
      |(minute == 20)
      |(minute == 25)
      |(minute == 30)
      |(minute == 35)
      |(minute == 40)
      |(minute == 45)
      |(minute == 50)
      |(minute == 55)){
         lightup(WordBijna, false, 1);
    }
    else{
         lightup(WordBijna, true, 1);
    }
}

void lightupHourMinute(int hour, int minute) {
    //First turn everything off.
    lightup(WordMinVijf, false, 1);
    lightup(WordMinTien, false, 1);
    lightup(WordKwart,   false, 1);
    lightup(WordTwintig, false, 1);
    lightup(WordMinuten, false, 1);
    lightup(WordHalf,    false, 1);
    lightup(WordOver,    false, 1);
    lightup(WordVoor,    false, 1);
    lightup(WordUur,     false, 1);
      
    if ((minute >= 0) && (minute <5)) {
        lightupHour(hour);
        lightup(WordUur,     true, 1);
    }
    else if ((minute >= 5) && (minute <10)) {
        lightup(WordMinVijf, true, 1);
        lightup(WordMinuten, true, 1);
        lightup(WordOver,    true, 1);
        lightupHour(hour);
    }
    else if ((minute >= 10) && (minute <15)) {
        lightup(WordMinTien, true, 1);
        lightup(WordMinuten, true, 1);
        lightup(WordOver,    true, 1);
        lightupHour(hour);
    }
    else if ((minute >= 15) && (minute <20)) {
        lightup(WordKwart,   true, 1);
        lightup(WordOver,    true, 1);
        lightupHour(hour);
    }
    else if ((minute >= 20) && (minute <25)) {
        lightup(WordMinTien, true, 1);
        lightup(WordMinuten, true, 1);
        lightup(WordVoor,    true, 1);
        lightup(WordHalf,    true, 1);
        lightupHour(hour + 1);
    }
    else if ((minute >= 25) && (minute <30)) {
        lightup(WordMinVijf, true, 1);
        lightup(WordMinuten, true, 1);
        lightup(WordVoor,    true, 1);
        lightup(WordHalf,    true, 1);
        lightupHour(hour + 1);
    }
    else if ((minute >= 30) && (minute <35)) {
        lightup(WordHalf,    true, 1);
        lightupHour(hour + 1);
    }
    else if ((minute >= 35) && (minute <40)) {
        lightup(WordMinVijf, true, 1);
        lightup(WordMinuten, true, 1);
        lightup(WordOver,    true, 1);
        lightup(WordHalf,    true, 1);
        lightupHour(hour + 1);
    }
    else if ((minute >= 40) && (minute <45)) {
        lightup(WordMinTien, true, 1);
        lightup(WordMinuten, true, 1);
        lightup(WordOver,    true, 1);
        lightup(WordHalf,    true, 1);
        lightupHour(hour + 1);
    }
    else if ((minute >= 45) && (minute <50)) {
        lightup(WordKwart,   true, 1);
        lightup(WordVoor,    true, 1);
        lightupHour(hour + 1);
    }
    else if ((minute >= 50) && (minute <55)) {
        lightup(WordMinTien, true, 1);
        lightup(WordMinuten, true, 1);
        lightup(WordVoor,    true, 1);
        lightupHour(hour + 1);
    }
    else if ((minute >= 55) && (minute <=59)) {
        lightup(WordMinVijf, true, 1);
        lightup(WordMinuten, true, 1);
        lightup(WordVoor,    true, 1);
        lightupHour(hour + 1);
    }
}

void lightupHour(int hour) {
    //First turn everything off.
    lightup(WordEen,    false, 1);
    lightup(WordTwee,   false, 1);
    lightup(WordDrie,   false, 1);
    lightup(WordVier,   false, 1);
    lightup(WordVijf,   false, 1);
    lightup(WordZes,    false, 1);
    lightup(WordZeven,  false, 1);
    lightup(WordAcht,   false, 1);
    lightup(WordNegen,  false, 1);
    lightup(WordTien,   false, 1);
    lightup(WordElf,    false, 1);
    lightup(WordTwaalf, false, 1);

    switch (hour) {
        case 1:
        case 13:
            lightup(WordEen, true, 1);
            break;
        case 2:
        case 14:
            lightup(WordTwee, true, 1);
            break;
        case 3:
        case 15:
            lightup(WordDrie, true, 1);
            break;
        case 4:
        case 16:
            lightup(WordVier, true, 1);
            break;
        case 5:
        case 17:
            lightup(WordVijf, true, 1);
            break;
        case 6:
        case 18:
            lightup(WordZes, true, 1);
            break;
        case 7:
        case 19:
            lightup(WordZeven, true, 1);
            break;
        case 8:
        case 20:
            lightup(WordAcht, true, 1);
            break;
        case 9:
        case 21:
            lightup(WordNegen, true, 1);
            break;
        case 10:
        case 22:
            lightup(WordTien, true, 1);
            break;
        case 11:
        case 23:
            lightup(WordElf, true, 1);
            break;
        case 00:
        case 12:
            lightup(WordTwaalf, true, 1);
            break;
    }
}

void lightup(int Word[], bool on, int dimmer_offset) {
  #if (USE_WS2812_CTYPE > 1)
    RgbwColor color;
    RgbwColor off;
  #else
    RgbColor color;
    RgbColor off;
  #endif

  float dimmer = 100 / (float)Settings.light_dimmer;
  color.R = ((Settings.light_color[0] / dimmer ) / dimmer_offset);
  color.G = ((Settings.light_color[1] / dimmer ) / dimmer_offset);
  color.B = ((Settings.light_color[2] / dimmer ) / dimmer_offset);

  off.R = 0;
  off.G = 0;
  off.B = 0;

  for (int x = 0; x < (int)Settings.light_pixels + 1; x++) {
    if(Word[x] == -1) {
      break;
    }
    else {
      strip->SetPixelColor(Word[x], (on ? color : off));
    }
  }
}

#endif  // USE_WS2812
#endif  // USE_LIGHT
