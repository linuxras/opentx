/**
 * INAV telemetry radar
 *
 * for CRSF and AFHDS2A telemetry (expects ibus_telemetry_type=0-3)
 *
 * 
 TODO:
 + lines and status bar,
 + model name, timer1, tx batt, rssi,
 - telemetry:
   + recognize telemetry type: crsf (by type) /ibus (by index) - by enabled module / flag?,
     telemetryProtocol == PROTOCOL_FLYSKY_IBUS / PROTOCOL_PULSES_CROSSFIRE
     https://github.com/iNavFlight/inav/blob/master/docs/Telemetry.md#available-smartport-sport-sensors
     https://github.com/iNavFlight/OpenTX-Telemetry-Widget/blob/master/src/SCRIPTS/TELEMETRY/iNav.lua
   + batt V, curr, alt, dist, speed,
   + FM, sats
   - GPS HDOP, "<9m","~85m",">90m"
   + store home GPS coords on long press OK,
   + GPS coords, heading,
   - GPS alt,
   - rotate coords to homeHeading,
   - North or homeHeading orientation modes toggle, by BIND?
 + draw home and craft:
   + calculate home and craft position BBox,
   + scale and translate to fit screen BBox,
   + draw home,
   + draw craft,
   + draw rotated craft arrow,
   + test,

afhds2a telem:
   - test
crsf telemetry:
   - test
 */
#include "opentx.h"

static const int8_t sine[32] = {
  0,24,48,70,90,106,117,125,127,125,117,106,90,70,48,24,0,-25,-49,-71,-91,-107,-118,-126,-128,-126,-118,-107,-91,-71,-49,-25
};

#define INAV_BATTP_POSX   30
#define INAV_BATTP_POSY    9
#define INAV_VOLT_POSX    LCD_W
#define INAV_VOLT_POSY     1
#define INAV_CELLV_POSX   30
#define INAV_CELLV_POSY   26
#define INAV_CURRENT_POSX 30
#define INAV_CURRENT_POSY 43

#define INAV_GSPD_POSX    19
#define INAV_GSPD_POSY    57

#define INAV_DIST_POSX    38
#define INAV_DIST_POSY    57

#define INAV_ALT_POSX     90
#define INAV_ALT_POSY     57

#define INAV_GALT_POSX    LCD_W
#define INAV_GALT_POSY    43

#define INAV_FM_POSX      (LCD_W / 2)
#define INAV_FM_POSY      9

#define INAV_SATS_POSX      LCD_W 
#define INAV_SATS_POSY      8

#define BBOX_CENTER_X (LCD_W / 2)
#define BBOX_CENTER_Y (36)
#define BBOX_SIZE     (21)

#define HOME_ICON '\xce'
#define SATS_ICON '\xd1'

struct Point2D {
  int8_t x;
  int8_t y;
};

struct InavData {
  int32_t homeLat;
  int32_t homeLon;
  int32_t currentLat;
  int32_t currentLon;
  uint8_t homeHeading;
  uint8_t heading;
};

static InavData inavData; // = (InavData *)&reusableBuffer.cToolData[0];

static Point2D rotate(Point2D *p, uint8_t angle) {
  Point2D rotated;
  int8_t sinVal = sine[angle];
  int8_t cosVal = sine[(angle + 8) & 0x1F];
  rotated.x = (p->x * cosVal - p->y * sinVal) >> 7;
  rotated.y = (p->y * cosVal + p->x * sinVal) >> 7;
  return rotated;
}

static void inavDrawHome(uint8_t x, uint8_t y) {
  lcdDrawChar(x - 2, y - 3, HOME_ICON);
}

// point left / right, tip is (0,0) and is not rotated
static void inavDrawCraft(uint8_t x, uint8_t y) {
  constexpr int8_t pLX = -3;
  constexpr int8_t pLY = 10;
  constexpr int8_t pRX =  3;
  constexpr int8_t pRY = 10;
  uint8_t angle = inavData.heading + inavData.homeHeading;
  int8_t sinVal = sine[angle];
  int8_t cosVal = sine[(angle + 8) & 0x1F];

  // rotate
  int8_t rotatedPLX = (pLX * cosVal - pLY * sinVal) >> 7;
  int8_t rotatedPLY = (pLY * cosVal + pLX * sinVal) >> 7;
  int8_t rotatedPRX = (pRX * cosVal - pRY * sinVal) >> 7;
  int8_t rotatedPRY = (pRY * cosVal + pRX * sinVal) >> 7;

  uint8_t tPLX = x + rotatedPLX;
  uint8_t tPLY = y + rotatedPLY;
  uint8_t tPRX = x + rotatedPRX;
  uint8_t tPRY = y + rotatedPRY;

  // translate and draw
  lcdDrawLine(x, y, tPLX, tPLY, SOLID, FORCE);
  lcdDrawLine(x, y, tPRX, tPRY, SOLID, FORCE);
  lcdDrawLine(tPLX, tPLY, tPRX, tPRY, DOTTED, FORCE);
  // lcdDrawChar(x - 2, y - 3, '*', SMLSIZE);
}

// Mode: 0 - Passthrough, 1-Armed(rate), 2-Horizon, 3-Angle, 4-Waypoint, 5-AltHold, 6-PosHold, 7-Rth, 8-Launch, 9-Failsafe
static void inavDrawMode(uint8_t mode) {
  static const char modeText[10][8] = {
    {'P','A','S','S','T','H','R','U'},
    {'A','R','M','E','D','\0',' ',' '},
    {'H','O','R','I','Z','O','N','\0'},
    {'A','N','G','L','E','\0',' ',' '},
    {'W','A','Y','P','O','I','N','T'},
    {'A','L','T',' ','H','O','L','D'},
    {'P','O','S',' ','H','O','L','D'},
    {'R','T','H','\0',' ',' ',' ',' '},
    {'L','A','U','N','C','H','\0',' '},
    {'F','A','I','L','S','A','F','E'},
  };

  lcdDrawSizedText(INAV_FM_POSX, INAV_FM_POSY, modeText[mode], 8, SMLSIZE | CENTERED);
}

static void inavDraw() {
  lcdDrawFilledRect(0, 0, LCD_W, FH, SOLID, 0);
  lcdDrawSolidVerticalLine(36, FH, LCD_H - FH, FORCE);
  lcdDrawSolidVerticalLine(LCD_W - 32, FH, LCD_H - FH, FORCE);
  lcdDrawSolidHorizontalLine(0, 55, 36, FORCE);
  lcdDrawSolidHorizontalLine(LCD_W - 32, 51, 32, FORCE);

  // Model Name
  putsModelName(0, 0, g_model.header.name, g_eeGeneral.currModel, INVERS);

  // Main Voltage (or alarm if any)
  putsVBat(LCD_W - 42, 1, (IS_TXBATT_WARNING() ? BLINK|INVERS : INVERS) | SMLSIZE);

  // Timer 1
  drawTimer(58, 1, timersStates[0].val, SMLSIZE | INVERS);

  uint8_t rxBatt = 0, rssi = 0, sats = 0, fix, hdop = 9, mode = 0;
  int32_t dist = 0, alt = 0, galt = 0, speed = 0;

  for (int i = 0; i < MAX_TELEMETRY_SENSORS; i++) {
    if (!isTelemetryFieldAvailable(i)) break;

    TelemetryItem & telemetryItem = telemetryItems[i];

    if (telemetryProtocol == PROTOCOL_PULSES_CROSSFIRE) {
      TelemetrySensor & sensor = g_model.telemetrySensors[i];

      if (strstr(sensor.label, ZSTR_RX_RSSI1)) { // RSSI
        rssi = telemetryItem.value;
      } else if (strstr(sensor.label, ZSTR_ALT)) { // Altitude
        alt = telemetryItem.value;
      } else if (strstr(sensor.label, ZSTR_GPSALT)) { // GPS altitude
        galt = telemetryItem.value;
      } else if (strstr(sensor.label, ZSTR_BATT_PERCENT)) { // batt percent
        drawValueWithUnit(INAV_BATTP_POSX, INAV_BATTP_POSY, telemetryItem.value, sensor.unit, DBLSIZE | RIGHT);
      } else if (strstr(sensor.label, ZSTR_A4)) { // average cell value
        drawValueWithUnit(INAV_CELLV_POSX, INAV_CELLV_POSY, telemetryItem.value, sensor.unit, PREC2 | DBLSIZE | RIGHT);
      } else if (strstr(sensor.label, ZSTR_BATT)) { // Voltage
        rxBatt = telemetryItem.value;
      } else if (strstr(sensor.label, ZSTR_CURR)) { // Current
        drawValueWithUnit(INAV_CURRENT_POSX, INAV_CURRENT_POSY, telemetryItem.value, sensor.unit, PREC1 | MIDSIZE | RIGHT);
      } else if (strstr(sensor.label, ZSTR_FLIGHT_MODE)) { // flight mode
        lcdDrawSizedText(INAV_FM_POSX, INAV_FM_POSY, telemetryItem.text, sizeof(telemetryItem.text), CENTERED);
      // } else if (sensor.id == TEMP2_ID) { // GPS lock status, accuracy, home reset trigger, and number of satellites.

//      } else if (strstr(sensor.label, ZSTR_DIST)) { // Distance
      } else if (strstr(sensor.label, ZSTR_DIST) || strstr(sensor.label, "0420")) { // Distance
        dist = telemetryItem.value;
      } else if (strstr(sensor.label, ZSTR_HDG)) { // Heading
        // inavData.heading = ((telemetryItem.value / (10 ^ sensor.prec)) * 100) / 1125;
        inavData.heading = convertTelemetryValue(telemetryItem.value, sensor.unit, sensor.prec, sensor.unit, 2) / 1125;
      } else if (strstr(sensor.label, ZSTR_GSPD)) { // GPS Speed
        speed = telemetryItem.value;
      } else if (strstr(sensor.label, ZSTR_SATELLITES)) { // GPS Sats
        // lcdDrawNumber(INAV_SATS_POSX, INAV_SATS_POSY, telemetryItem.value, MIDSIZE | RIGHT);
        sats = telemetryItem.value;

        // Fake CRSF HDOP
        // data.hdop = math.floor(data.satellites * 0.01) % 10
        // text(72, 59, (data.hdop == 0 and not data.gpsFix) and "---" or (9 - data.hdop) * 0.5 + 0.8, data.set_flags(RIGHT, tmp))
        hdop = 9 - (sats % 10);
      } else if (strstr(sensor.label, ZSTR_GPS)) { // GPS coords
        inavData.currentLat = telemetryItem.gps.longitude;
        inavData.currentLon = telemetryItem.gps.latitude;
      }

    } else if (telemetryProtocol == PROTOCOL_FLYSKY_IBUS) {

      rssi = telemetryData.rssi.value;

      switch(g_model.telemetrySensors[i].instance) { // inav index - 1
        case 1: // voltage sensor
          rxBatt = telemetryItem.value; // scale down to PREC1

          // additionally draw in place of cell voltage
          drawValueWithUnit(INAV_CELLV_POSX, INAV_CELLV_POSY, rxBatt, UNIT_VOLTS, PREC1 | DBLSIZE | RIGHT);
          break;
        case 3: // Status
          sats = telemetryItem.value / 1000;
          fix = (telemetryItem.value / 100) - sats * 10;
          hdop = (telemetryItem.value / 10) - (sats * 100) - (fix * 10);
          mode = telemetryItem.value - (sats * 1000) - (fix * 100) - (hdop * 10);

          inavDrawMode(mode);
          break;
        case 4: // Course in degree - store for drawing
          inavData.heading = telemetryItem.value / 1125; // div by 5.625 => 64 degrees
          break;
        case 5: // Current in Amperes
            drawValueWithUnit(INAV_CURRENT_POSX, INAV_CURRENT_POSY, telemetryItem.value, UNIT_AMPS, PREC2 | MIDSIZE | RIGHT);
          break;
        case 6: // Altitude
          alt = (int16_t)(telemetryItem.value) / 100;
          break;
        case 10: // GPS Altitude - something is not right here
          galt = (int16_t)(telemetryItem.value) / 100;
          break;
        case 8: // Distance
          dist = telemetryItem.value;
          break;
        case 11: // 12.Second part of Lattitude (Rpm type), for example 5678 (-12.3456789 N).
          inavData.currentLat = (inavData.currentLat & 0xffff0000) | telemetryItem.value;
          break;
        case 12: // 13.Second part of Longitude (Rpm type), for example 6789 (-123.4567891 E).
          inavData.currentLon = (inavData.currentLon & 0xffff0000) | telemetryItem.value;
          break;
        case 13: // 14.First part of Lattitude (Voltage type), for example -12.45 (-12.3456789 N).
          inavData.currentLat = (inavData.currentLat & 0x0000ffff) | (telemetryItem.value << 16);
          break;
        case 14: // 15.First part of Longitude (Voltage type), for example -123.45 (-123.4567890 E).
          inavData.currentLon = (inavData.currentLon & 0x0000ffff) | (telemetryItem.value << 16);
          break;
        case 15: // GPS Speed
          speed = telemetryItem.value / 10;
          break;
      }
    }
  }

  // When GPS accuracy (HDOP) is displayed as a decimal, the range is 0.8 - 5.3 and it's rounded to the nearest 0.5 HDOP.
  // This is due to HDOP being sent as a single integer from 0 to 9, not as the actual HDOP decimal value (not applicable to Crossfire)
  lcdDrawText(LCD_W, INAV_SATS_POSY + 14, "HDOP", SMLSIZE | RIGHT);
  lcdDrawNumber(LCD_W, INAV_SATS_POSY + 21, hdop * 5 + 8, PREC1 | MIDSIZE | RIGHT);

  drawValueWithUnit(LCD_W - 6, 1, rxBatt / 10, UNIT_VOLTS, PREC1 | SMLSIZE | INVERS | RIGHT);

  drawValueWithUnit(LCD_W - 11, 53, rssi, UNIT_DB, MIDSIZE | RIGHT);
  drawValueWithUnit(INAV_GSPD_POSX, INAV_GSPD_POSY, speed, UNIT_KMH, PREC1 | RIGHT);

  drawValueWithUnit(INAV_DIST_POSX, INAV_DIST_POSY, dist, UNIT_METERS, 0);
  drawValueWithUnit(INAV_ALT_POSX, INAV_ALT_POSY, alt, UNIT_METERS, RIGHT);

  lcdDrawChar(INAV_SATS_POSX - 28, INAV_SATS_POSY + 4, SATS_ICON);
  lcdDrawNumber(INAV_SATS_POSX, INAV_SATS_POSY, sats, MIDSIZE | RIGHT);
  drawValueWithUnit(INAV_GALT_POSX, INAV_GALT_POSY, galt, UNIT_METERS, RIGHT);

  // lcdDrawText(5, 45, "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2");

  // lcdDrawNumber(70, 20, inavData.currentLat, SMLSIZE | RIGHT);
  // lcdDrawNumber(70, 30, inavData.currentLon, SMLSIZE | RIGHT);

  int32_t h = inavData.homeLat - inavData.currentLat;
  int32_t w = inavData.homeLon - inavData.currentLon;
  int32_t d = isqrt32((w * w) + (h * h));

  int32_t scaleFactor = limit<int32_t>(1, (d / BBOX_SIZE), INT16_MAX); // TODO: while h || w > BBOX_SIZE do h /= 2; w /=2 ?

  // calculate center
  int32_t centerLon = (inavData.homeLon + inavData.currentLon) / 2;
  int32_t centerLat = (inavData.homeLat + inavData.currentLat) / 2;

  // translate to center
  int32_t translatedHomeLon = inavData.homeLon - centerLon;
  int32_t translatedHomeLat = inavData.homeLat - centerLat;
  int32_t translatedCurrentLon = inavData.currentLon - centerLon;
  int32_t translatedCurrentLat = inavData.currentLat - centerLat;

  // rotate to homeHeading
  // ...

  // scale
  int8_t scaledHomeLon = translatedHomeLon / scaleFactor;
  int8_t scaledHomeLat = translatedHomeLat / scaleFactor;
  int8_t scaledCurrentLon = translatedCurrentLon / scaleFactor;
  int8_t scaledCurrentLat = translatedCurrentLat / scaleFactor;

  // translate to LCD center space and draw
  inavDrawHome(BBOX_CENTER_X + scaledHomeLon, BBOX_CENTER_Y + scaledHomeLat);
  inavDrawCraft(BBOX_CENTER_X + scaledCurrentLon, BBOX_CENTER_Y + scaledCurrentLat);
}

void inavRun(event_t event) {
  if (event != 0xff) {
    globalData.cToolRunning = 1;
    lcdClear(); // when run as telemetry screen clear is done elsewhere
  }

  if (event == EVT_KEY_LONG(KEY_EXIT)) { // exit on long press CANCEL
    globalData.cToolRunning = 0;
    popMenu();
  } else if (inavData.homeLat == 0 || event == EVT_KEY_LONG(KEY_ENTER)) { // set home on init or long press OK
    inavData.homeLat = inavData.currentLat;
    inavData.homeLon = inavData.currentLon;
    inavData.homeHeading = inavData.heading;
  }

  // static uint16_t frame = 0;
  // if (frame++ & 1) { // every other frame to reduce CPU load
    inavDraw();
  // }
}
