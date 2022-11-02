/**
 * ExpressLRS V3 configurator for i6X based on elrsV2/3.lua
 * @author Jan Kozak (ajjjjjjjj)
 *
 * Limitations vs elrsV3.lua:
 * - no some int, float, string fields support, but not used by ExpressLRS anyway,
 */

#include "opentx.h"
#include "tiny_string.cpp"

#define PACKED __attribute__((packed))

#define TYPE_UINT8				   0
#define TYPE_INT8				     1
#define TYPE_UINT16				   2
#define TYPE_INT16				   3
#define TYPE_FLOAT				   8
#define TYPE_TEXT_SELECTION  9
#define TYPE_STRING				  10
#define TYPE_FOLDER				  11
#define TYPE_INFO				    12
#define TYPE_COMMAND			  13
#define TYPE_BACK           14
#define TYPE_DEVICE         15
#define TYPE_DEVICES_FOLDER 16

extern uint8_t cScriptRunning;

struct FieldProps {
  uint8_t nameOffset;
  uint8_t nameLength;
  uint8_t valuesOffset; // valueOffset|min|timeout for commands
  uint8_t valuesLength; // valuesLength|max|lastStatus for popup
  uint8_t unitOffset;
  uint8_t unitLength;
  uint8_t parent;
  uint8_t type;
  uint8_t value;
  uint8_t id;
  // uint8_t hidden : 1;
} PACKED;

struct FieldFunctions {
  void (*load)(FieldProps*, uint8_t *, uint8_t);
  void (*save)(FieldProps*);
  void (*display)(FieldProps*, uint8_t, uint8_t);
};

static constexpr uint8_t NAMES_BUFFER_SIZE  = 204; // 191 - 12 = ~180+ (no Antenna Mode with FM30)
static constexpr uint8_t VALUES_BUFFER_SIZE = 255; // 154+
static uint8_t *namesBuffer = &reusableBuffer.MSC_BOT_Data[0];
uint8_t namesBufferOffset = 0;
static uint8_t *valuesBuffer = &reusableBuffer.MSC_BOT_Data[NAMES_BUFFER_SIZE];
//static uint8_t valuesBuffer[VALUES_BUFFER_SIZE];
uint8_t valuesBufferOffset = 0;

// last 25b are also used for popup messages
static constexpr uint8_t FIELD_DATA_BUFFER_SIZE = 172; // 172+
//static uint8_t *fieldData = &reusableBuffer.MSC_BOT_Data[NAMES_BUFFER_SIZE];
static uint8_t fieldData[FIELD_DATA_BUFFER_SIZE];
static constexpr uint8_t POPUP_MSG_OFFSET = FIELD_DATA_BUFFER_SIZE - 24 - 1;
// static uint8_t fieldData[FIELD_DATA_BUFFER_SIZE];
uint8_t fieldDataLen = 0;

static constexpr uint8_t FIELDS_MAX_COUNT = 17; // 15 + Antenna Mode + Airport // 32 * 8 = 256b // 30 + 2 margin for future fields
static FieldProps fields[FIELDS_MAX_COUNT];
//static FieldProps *fields = (FieldProps *)&reusableBuffer.MSC_BOT_Data[NAMES_BUFFER_SIZE + FIELD_DATA_BUFFER_SIZE];
uint8_t allocatedFieldsCount = 0;

static constexpr uint8_t DEVICES_MAX_COUNT = 8;
static uint8_t *deviceIds = &reusableBuffer.MSC_BOT_Data[NAMES_BUFFER_SIZE + VALUES_BUFFER_SIZE];
//static uint8_t deviceIds[DEVICES_MAX_COUNT];
uint8_t devicesLen = 0;
uint8_t otherDevicesId = 255;

#define BTN_NONE 0
#define BTN_REQUESTED 1
#define BTN_ADDED 2
uint8_t otherDevicesState = BTN_NONE;

uint8_t deviceId = 0xEE;
uint8_t handsetId = 0xEF;

static constexpr uint8_t DEVICE_NAME_MAX_LEN = 20;
static uint8_t *deviceName = &reusableBuffer.MSC_BOT_Data[NAMES_BUFFER_SIZE + VALUES_BUFFER_SIZE + DEVICES_MAX_COUNT];
//static char deviceName[DEVICE_NAME_MAX_LEN];
uint8_t lineIndex = 1;
uint8_t pageOffset = 0;
uint8_t edit = 0;
uint8_t charIndex = 1;
static FieldProps * fieldPopup = 0;
tmr10ms_t fieldTimeout = 0;
uint8_t fieldId = 1;
uint8_t fieldChunk = 0;

static char goodBadPkt[11] = "?/???    ?";
uint8_t elrsFlags = 0;
static char *elrsFlagsInfo = (char *)&reusableBuffer.MSC_BOT_Data[NAMES_BUFFER_SIZE + VALUES_BUFFER_SIZE + DEVICES_MAX_COUNT + DEVICE_NAME_MAX_LEN]; // 16
//static char elrsFlagsInfo[16] = "";
uint8_t expectedFieldsCount = 0;
uint8_t backButtonId = 2;
tmr10ms_t devicesRefreshTimeout = 50;
uint8_t allParamsLoaded = 0;
uint8_t folderAccess = 0; // folder id
uint8_t statusComplete = 0;
int8_t expectedChunks = -1;
uint8_t deviceIsELRS_TX = 0;
tmr10ms_t linkstatTimeout = 100;
uint8_t titleShowWarn = 0;
tmr10ms_t titleShowWarnTimeout = 100;
uint8_t reloadFolder = 0;

static constexpr uint8_t COL2          = 70;
static constexpr uint8_t maxLineIndex  =  6;
static constexpr uint8_t textXoffset   =  0;
static constexpr uint8_t textYoffset   =  3;
static constexpr uint8_t textSize      =  8;

#define tostring(c)       (char)(c + 48)
#define getTime           get_tmr10ms
#define EVT_VIRTUAL_EXIT  EVT_KEY_BREAK(KEY_EXIT)
#define EVT_VIRTUAL_ENTER EVT_KEY_BREAK(KEY_ENTER)
#define EVT_VIRTUAL_NEXT  EVT_KEY_FIRST(KEY_DOWN)
#define EVT_VIRTUAL_PREV  EVT_KEY_FIRST(KEY_UP)

static constexpr uint8_t RESULT_OK = 2;
static constexpr uint8_t RESULT_CANCEL = 1;

static void luaLcdDrawGauge(coord_t x, coord_t y, coord_t w, coord_t h, int32_t val, int32_t max) {
  lcdDrawRect(x, y, w+1, h, 0xff);
  uint8_t len = limit((uint8_t)1, uint8_t(w*val/max), uint8_t(w));
  lcdDrawSolidFilledRect(x+1, y+1, len, h-2);
}

static void storeField(FieldProps * field);
static void clearFields();
static void addBackButton();
static void reloadAllField();
static FieldProps * getField(uint8_t line);
static void UIbackExec(FieldProps * field);
static void parseDeviceInfoMessage(uint8_t* data);
static void parseParameterInfoMessage(uint8_t* data, uint8_t length);
static void parseElrsInfoMessage(uint8_t* data);
static void refreshNext(uint8_t command, uint8_t* data, uint8_t length);
static void runPopupPage(event_t event);
static void runDevicePage(event_t event);
static void lcd_title();
static void lcd_warn();
static void handleDevicePageEvent(event_t event);
static void fieldTextSelectionSave(FieldProps * field);


static void crossfireTelemetryPush4(const uint8_t cmd, const uint8_t third, const uint8_t fourth) {
//  TRACE("crsf push %x  %x  %x", cmd, third, fourth);
  uint8_t crsfPushData[4] { deviceId, handsetId, third, fourth };
  crossfireTelemetryPush(cmd, crsfPushData, 4);
}

static void crossfireTelemetryPing(){
  const uint8_t crsfPushData[2] = { 0x00, 0xEA };
  crossfireTelemetryPush(0x28, (uint8_t *) crsfPushData, 2);
}

static void clearFields() {
//  TRACE("clearFields %d", allocatedFieldsCount);
  for (uint32_t i = 0; i < allocatedFieldsCount; i++) {
    fields[i].nameLength = 0;
    fields[i].valuesLength = 0;
  }
  otherDevicesState = BTN_NONE;
  allocatedFieldsCount = 0;
}

// Both buttons must be added as last ones because i cannot overwrite existing Id
static void addBackButton() {
  backButtonId = 100; // cannot be allocatedFieldsCount because can overwrite existing id
  FieldProps backBtnField;
  backBtnField.id = backButtonId;
  backBtnField.nameLength = 1;
  backBtnField.type = TYPE_BACK;
  backBtnField.parent = (folderAccess == 0) ? 255 : folderAccess;
  storeField(&backBtnField);
}

static void addOtherDevicesButton() {
  otherDevicesId = 101; // cannot be allocatedFieldsCount because can overwrite existing id
  FieldProps otherDevicesField;
  otherDevicesField.id = otherDevicesId;
  otherDevicesField.nameLength = 1;
  otherDevicesField.type = TYPE_DEVICES_FOLDER;
  otherDevicesField.parent = 0;
  storeField(&otherDevicesField);
  otherDevicesState = BTN_ADDED;
}

static void reloadAllField() {
//  TRACE("reloadAllField");
  allParamsLoaded = 0;
  fieldId = 1;
  fieldChunk = 0;
  fieldDataLen = 0;
  namesBufferOffset = 0;
  valuesBufferOffset = 0;
}

static FieldProps * getFieldById(const uint8_t id) {
  for (uint32_t i = 0; i < allocatedFieldsCount; i++) {
    FieldProps * field = &fields[i];
    if (id == field->id) {
      return field;
    }
  }
  return nullptr;
}

/**
 * Store field at its location or add new one if not found.
 */
static void storeField(FieldProps * field) {
  TRACE("storeField id %d", field->id);
  FieldProps * storedField = getFieldById(field->id);
  if (storedField == nullptr) {
    storedField = &fields[allocatedFieldsCount];
    allocatedFieldsCount++;
    TRACE("allocFieldsCount %d", allocatedFieldsCount);
  }
  memcpy(storedField, field, sizeof(FieldProps));
}

/**
 * Get field from line index taking only loaded current folder fields into account.
 */
static FieldProps * getField(const uint8_t line) {
  return &fields[line - 1];
}

static uint8_t getSemicolonCount(const char * str, const uint8_t len) {
  uint8_t count = 0;
  for (uint32_t i = 0; i < len; i++) {
    if (str[i] == ';') count++;
  }
  return count;
}

static void incrField(int8_t step) {
  FieldProps * field = getField(lineIndex);
  int32_t min = 0, max = 0;
  if (field->type <= TYPE_INT16) {
      min = field->valuesOffset;
      max = field->valuesLength;
  } else if (field->type == TYPE_TEXT_SELECTION) {
//    min = 0;
    max = getSemicolonCount((char *)&valuesBuffer[field->valuesOffset], field->valuesLength);
  }
  field->value = limit<int32_t>(min, field->value + step, max);
}

static void selectField(int8_t step) {
  int8_t newLineIndex = lineIndex;
  FieldProps * field;
  do {
    newLineIndex = newLineIndex + step;
    if (newLineIndex <= 0) {
      newLineIndex = allocatedFieldsCount;
    } else if (newLineIndex == 1 + allocatedFieldsCount) {
      newLineIndex = 1;
      pageOffset = 0;
    }
    field = getField(newLineIndex);
  } while (newLineIndex != lineIndex && (field == 0 || field->nameLength == 0));
  lineIndex = newLineIndex;
  if (lineIndex > maxLineIndex + pageOffset) {
    pageOffset = lineIndex - maxLineIndex;
  } else if (lineIndex <= pageOffset) {
    pageOffset = lineIndex - 1;
  }
}

static uint8_t getDevice(uint8_t devId) {
  TRACE("getDevice %x", devId);
  for (uint8_t i = 0; i < devicesLen; i++) {
    if (deviceIds[i] == devId) {
      return deviceIds[i];
    }
  }
  return 0;
}

static void strShorten(char * src, const uint8_t maxLen) {
  int8_t diff = 0;
  char * srcStrPtr = src;
  char * srcStrPtr2;
  while ((srcStrPtr2 = strstr(srcStrPtr, ";"))) {
      uint8_t itemLen = srcStrPtr2 - srcStrPtr;
      diff = itemLen - maxLen;
      if (diff > 0) {
        strcpy(srcStrPtr + maxLen, srcStrPtr2);
        srcStrPtr2 -= diff;
      }
      srcStrPtr = srcStrPtr2 + 1;
  }
}

static void unitSave(FieldProps * field, uint8_t * data, uint8_t unitOffset) {
  uint8_t unitLen = strlen((char*)&data[unitOffset]);
  field->unitLength = unitLen;
  if (field->type < TYPE_STRING && unitLen > 0) {
    memcpy(&namesBuffer[namesBufferOffset], (char*)&data[unitOffset], unitLen);
    field->unitOffset = namesBufferOffset;
    namesBufferOffset += unitLen;
  }
}

// UINT8
static void fieldUint8Display(FieldProps * field, uint8_t y, uint8_t attr) {
  char stringTmp[24];
  tiny_sprintf((char *)&stringTmp, "%u%s", 3, field->value, field->unitLength, (char *)&namesBuffer[field->unitOffset]);
  lcdDrawText(COL2, y, (char *)&stringTmp, attr);
}

static void fieldUint8Load(FieldProps * field, uint8_t * data, uint8_t offset) {
  field->value = data[offset + 0];
  field->valuesOffset = data[offset + 1]; // min
  field->valuesLength = data[offset + 2]; // max
  unitSave(field, data, offset + 4);
}

static void fieldUint8Save(FieldProps * field) {
  fieldTextSelectionSave(field);
}

// TEXT SELECTION
/**
 * Reused also for INFO fields value (i.e. commit sha) for 0 flash cost
 */
static void fieldTextSelectionLoad(FieldProps * field, uint8_t * data, uint8_t offset) {
  uint8_t len = strlen((char*)&data[offset]);
  field->value = data[offset + len + 1];
  unitSave(field, data, offset + len + 5);
  strShorten((char*)&data[offset], 12);
  len = strlen((char*)&data[offset]);
  if (field->valuesLength == 0) {
    memcpy(&valuesBuffer[valuesBufferOffset], (char*)&data[offset], len);
    field->valuesOffset = valuesBufferOffset;
    field->valuesLength = len;
    valuesBufferOffset += len;
  }
}

static void fieldTextSelectionSave(FieldProps * field) {
  crossfireTelemetryPush4(0x2D, field->id, field->value);
}

static uint8_t semicolonPos(const char * str, uint8_t last) {
  uint8_t pos = 0;
  while ((str[pos] != ';') && (pos < last)) pos++;
  return pos + 1;
}

static void fieldTextSelectionDisplay(FieldProps * field, uint8_t y, uint8_t attr) {
  uint8_t start = field->valuesOffset;
  uint8_t len;
  uint32_t i = 0;
  while (i++ < field->value) {
    start += semicolonPos((char *)&valuesBuffer[start], field->valuesLength - (start - field->valuesOffset));
    if (start - field->valuesOffset >= field->valuesLength) {
      lcdDrawText(COL2, y, "ERR", attr);
      return;
    }
  }
  len = semicolonPos((char *)&valuesBuffer[start], field->valuesLength - (start - field->valuesOffset)) - 1;

  char stringTmp[24];
  tiny_sprintf((char *)&stringTmp, "%s%s", 4, len, (char *)&valuesBuffer[start], field->unitLength, (char *)&namesBuffer[field->unitOffset]);
  lcdDrawText(COL2, y, (char *)&stringTmp, attr);
}

static void fieldStringDisplay(FieldProps * field, uint8_t y, uint8_t attr) {
  lcdDrawSizedText(COL2, y, (char *)&valuesBuffer[field->valuesOffset], field->valuesLength, attr);
}

static void fieldFolderOpen(FieldProps * field) {
  TRACE("fieldFolderOpen %d", field->id);
  lineIndex = 1;
  pageOffset = 0;
  folderAccess = field->id;
  clearFields();
  reloadAllField();
}

static void fieldFolderDeviceOpen(FieldProps * field) {
  // if folderAccess == devices folder, store only devices instead of fields
  expectedFieldsCount = devicesLen;
  devicesLen = 0;
  crossfireTelemetryPing(); //broadcast with standard handset ID to get all node respond correctly
  fieldFolderOpen(field);
}

static void noopLoad(FieldProps * field, uint8_t * data, uint8_t offset) {}
static void noopSave(FieldProps * field) {}
static void noopDisplay(FieldProps * field, uint8_t y, uint8_t attr) {}

static void fieldCommandLoad(FieldProps * field, uint8_t * data, uint8_t offset) {
  field->value = data[offset];
  field->valuesOffset = data[offset+1];
  strcpy((char *)&fieldData[POPUP_MSG_OFFSET], (char *)&data[offset+2]);
  if (field->value == 0) {
    fieldPopup = 0;
  }
}

static void fieldCommandSave(FieldProps * field) {
  if (field->value < 4) {
    field->value = 1;
    fieldTextSelectionSave(field); //crossfireTelemetryPush4(0x2D, field->id, field->value);
    fieldPopup = field;
    fieldPopup->valuesLength = 0;
    fieldTimeout = getTime() + field->valuesOffset;
  }
}

static void fieldUnifiedDisplay(FieldProps * field, uint8_t y, uint8_t attr) {
  const char* backPat = "[----BACK----]";
  const char* folderPat = "> %s";
  const char* otherPat = "> Other Devices";
  const char* cmdPat = "[%s]";
  const char *pat;
  uint8_t textIndent = textXoffset + 9;
  if (field->type == TYPE_FOLDER) {
    pat = folderPat;
    textIndent = textXoffset;
  } else if (field->type == TYPE_DEVICES_FOLDER) {
    pat = otherPat;
    textIndent = textXoffset;
  } else if (field->type == TYPE_BACK) {
    pat = backPat;
  } else { // CMD || DEVICE
    pat = cmdPat;
  }
  char stringTmp[24];
  tiny_sprintf((char *)&stringTmp, pat, 2, field->nameLength, (char *)&namesBuffer[field->nameOffset]);
  lcdDrawText(textIndent, y, (char *)&stringTmp, attr | BOLD);
}

static void UIbackExec(FieldProps * field = 0) {
  folderAccess = 0;
  clearFields();
  reloadAllField();
  devicesLen = 0;
  expectedFieldsCount = 0;
}

static void changeDeviceId(uint8_t devId) { //change to selected device ID
  TRACE("changeDeviceId %x", devId);
  folderAccess = 0;
  deviceIsELRS_TX = 0;
  elrsFlags = 0;
  //if the selected device ID (target) is a TX Module, we use our Lua ID, so TX Flag that user is using our LUA
  if (devId == 0xEE) {
    handsetId = 0xEF;
  } else { //else we would act like the legacy lua
    handsetId = 0xEA;
  }
  deviceId = devId;
  expectedFieldsCount = 0; //set this because next target wouldn't have the same count, and this trigger to request the new count
}

static void fieldDeviceIdSelect(FieldProps * field) {
//  TRACE("fieldDeviceIdSelect %x", field->id);
 changeDeviceId(field->id);
 crossfireTelemetryPing();
}

static void parseDeviceInfoMessage(uint8_t* data) {
  uint8_t offset;
  uint8_t id = data[2];
// TRACE("parseDevInfoMsg %x folderAcs %d, expect %d, devsLen %d", id, folderAccess, expectedFieldsCount, devicesLen);
  offset = strlen((char*)&data[3]) + 1 + 3;
  uint8_t devId = getDevice(id);
  if (!devId) {
    deviceIds[devicesLen] = id;
    if (folderAccess == otherDevicesId) { // if "Other Devices" opened store devices to fields
      FieldProps deviceField;
      deviceField.id = id;
      deviceField.type = TYPE_DEVICE;
      deviceField.nameLength = offset - 4;
      deviceField.nameOffset = namesBufferOffset;

      deviceField.parent = (id == deviceId) ? 255 : devicesLen/*otherDevicesId*/; // hide current device or set parent to "Other Devices"
      memcpy(&namesBuffer[namesBufferOffset], &data[3], deviceField.nameLength);
      namesBufferOffset += deviceField.nameLength;
      storeField(&deviceField);
      if (devicesLen == expectedFieldsCount - 1) {
        allParamsLoaded = 1;
        fieldId = 1;
        addBackButton();
      }
    }
    devicesLen++;
  }

  if (deviceId == id && folderAccess != otherDevicesId) {
    memcpy(&deviceName[0], (char *)&data[3], DEVICE_NAME_MAX_LEN);
    deviceIsELRS_TX = ((memcmp(&data[offset], "ELRS", 4) == 0) && (deviceId == 0xEE)) ? 1 : 0; // SerialNumber = 'E L R S' and ID is TX module
    uint8_t newFieldCount = data[offset+12];
//    TRACE("deviceId match %x, newFieldCount %d", deviceId, newFieldCount);
    reloadAllField();
    if (newFieldCount != expectedFieldsCount || newFieldCount == 0) {
      expectedFieldsCount = newFieldCount;
      clearFields();
      if (newFieldCount == 0) {
      // This device has no fields so the Loading code never starts
        allParamsLoaded = 1;
        fieldId = 1;
        addBackButton(); // createDeviceFields();
      }
    }
  }
}

static const FieldFunctions functions[] = {
  { .load=fieldUint8Load, .save=fieldUint8Save, .display=fieldUint8Display }, // 1 UINT8(0)
  { .load=noopLoad, .save=noopSave, .display=noopDisplay }, // 2 INT8(1)
  { .load=noopLoad, .save=noopSave, .display=noopDisplay }, // 3 UINT16(2)
  { .load=noopLoad, .save=noopSave, .display=noopDisplay }, // 4 INT16(3)
  { .load=noopLoad, .save=noopSave, .display=noopDisplay }, // 5 UINT32(4)
  { .load=noopLoad, .save=noopSave, .display=noopDisplay }, // 6 INT32(5)
  { .load=noopLoad, .save=noopSave, .display=noopDisplay }, // 7 UINT64(6)
  { .load=noopLoad, .save=noopSave, .display=noopDisplay }, // 8 INT64(7)
  { .load=noopLoad, .save=noopSave, .display=noopDisplay }, // 9 FLOAT(8)
  { .load=fieldTextSelectionLoad, .save=fieldTextSelectionSave, .display=fieldTextSelectionDisplay }, // 10 TEXT SELECTION(9)
  { .load=noopLoad, .save=noopSave, .display=fieldStringDisplay }, // 11 STRING(10)editing NOTIMPL
  { .load=noopLoad, .save=fieldFolderOpen, .display=fieldUnifiedDisplay }, // 12 FOLDER(11)
  { .load=fieldTextSelectionLoad, .save=noopSave, .display=fieldStringDisplay }, // 13 INFO(12)
  { .load=fieldCommandLoad, .save=fieldCommandSave, .display=fieldUnifiedDisplay }, // 14 COMMAND(13)
  { .load=noopLoad, .save=UIbackExec, .display=fieldUnifiedDisplay }, // 15 back(14)
  { .load=noopLoad, .save=fieldDeviceIdSelect, .display=fieldUnifiedDisplay }, // 16 device(15)
  { .load=noopLoad, .save=fieldFolderDeviceOpen, .display=fieldUnifiedDisplay }, // 17 deviceFOLDER(16)
};

static void parseParameterInfoMessage(uint8_t* data, uint8_t length) {
  if (data[2] != deviceId || data[3] != fieldId) {
    fieldDataLen = 0;
    fieldChunk = 0;
    return;
  }
  if (fieldDataLen == 0) {
    expectedChunks = -1;
  }
  if (fieldId == reloadFolder) { // if we finally receive the folder id, reset the pending reload folder flag
    reloadFolder = 0;
  }

  // Get by id or use temporary one to decide later if it should be stored
  FieldProps tempField = {0};
  FieldProps* field = getFieldById(fieldId);
  if (field == nullptr) {
    field = &tempField;
  }

  uint8_t chunksRemain = data[4];
  // If no field or the chunksRemain changed when we have data, don't continue
  if (/*field == 0 ||*/ (chunksRemain != expectedChunks && expectedChunks != -1)) {
    return;
  }
  expectedChunks = chunksRemain - 1;
  for (uint32_t i = 5; i < length; i++) {
    if (fieldDataLen > FIELD_DATA_BUFFER_SIZE) {
      TRACE("fieldData OF");
      return;
    }
    fieldData[fieldDataLen++] = data[i];
  }
//  TRACE("chunk len %d", length); // to know what is the max single chunk size

  if (chunksRemain > 0) {
    fieldChunk = fieldChunk + 1;
    statusComplete = 0;
  } else {
    TRACE("%d, %s, %d", fieldId, &fieldData[2], fieldDataLen);
    DUMP(fieldData, fieldDataLen);
    fieldChunk = 0;
    if (fieldDataLen < 4) {
      fieldDataLen = 0;
      return;
    }

    field->id = fieldId;
    uint8_t parent = fieldData[0];
    uint8_t type = fieldData[1] & 0x7F;
    uint8_t hidden = (fieldData[1] & 0x80) ? 1 : 0;
    uint8_t offset;

    if (type > TYPE_COMMAND) {
      TRACE("type %d", type);
      return;
    }

    if (field->nameLength != 0) {
      if (field->parent != parent || field->type != type/* || field->hidden != hidden*/) {
        fieldDataLen = 0;
        return;
      }
    }
    field->parent = parent;
    field->type = type;
    // field->hidden = hidden;
    offset = strlen((char*)&fieldData[2]) + 1 + 2;

    if (parent != folderAccess) {
      field->nameLength = 0; // mark as clear
    } else if (!hidden) {
      if (field->nameLength == 0) {
        field->nameLength = offset - 3;
        field->nameOffset = namesBufferOffset;
        memcpy(&namesBuffer[namesBufferOffset], &fieldData[2], field->nameLength);
        namesBufferOffset += field->nameLength;
      }
      functions[field->type].load(field, fieldData, offset);
      storeField(field);
    }

    if (fieldPopup == 0) {
      if (fieldId == expectedFieldsCount) { // if we have loaded all params
        TRACE("namesBufferOffset %d", namesBufferOffset);
        DUMP(namesBuffer, NAMES_BUFFER_SIZE);
        TRACE("valuesBufferOffset %d", valuesBufferOffset);
        DUMP(valuesBuffer, VALUES_BUFFER_SIZE);
        TRACE("allocatedFieldsCount %d", allocatedFieldsCount);
        allParamsLoaded = 1;
        fieldId = 1;
        if (folderAccess != 0) {
          addBackButton();
        } else {
          otherDevicesState = BTN_REQUESTED;
        }
      } else if (allParamsLoaded == 0) {
        fieldId++; // fieldId = 1 + (fieldId % (fieldsLen-1));
      } else if (reloadFolder != 0) { // if we still have to reload the folder name
        fieldId = reloadFolder;
        fieldChunk = 0;
        statusComplete = 0;
      }
      fieldTimeout = getTime() + 200;
    } else {
      fieldTimeout = getTime() + fieldPopup->valuesOffset;
    }
    if (reloadFolder == 0) {
      statusComplete = 1;  // status is not complete, we got to reload the folder
    }
    fieldDataLen = 0;
  }
}

static void parseElrsInfoMessage(uint8_t* data) {
  if (data[2] != deviceId) {
    fieldDataLen = 0;
    fieldChunk = 0;
    return;
  }

  uint8_t badPkt = data[3];
  uint16_t goodPkt = (data[4]*256) + data[5];
  uint8_t newFlags = data[6];
  // If flags are changing, reset the warning timeout to display/hide message immediately
  if (newFlags != elrsFlags) {
    elrsFlags = newFlags;
    titleShowWarnTimeout = 0;
  }
  strcpy(elrsFlagsInfo, (char*)&data[7]);

  char state = (elrsFlags & 1) ? 'C' : '-';
  tiny_sprintf(goodBadPkt, "%u/%u   %c", 3, badPkt, goodPkt, state);
}

static void refreshNext(uint8_t command = 0, uint8_t* data = 0, uint8_t length = 0) {
  if (command == 0x29) {
    parseDeviceInfoMessage(data);
  } else if (command == 0x2B && folderAccess != otherDevicesId /* !devicesFolderOpened */) {
    parseParameterInfoMessage(data, length);
    if (allParamsLoaded < 1 || statusComplete == 0) {
      fieldTimeout = 0;
    }
  } else if (command == 0x2E) {
    parseElrsInfoMessage(data);
  }

  tmr10ms_t time = getTime();
  if (fieldPopup != 0) {
    if (time > fieldTimeout && fieldPopup->value != 3) {
      crossfireTelemetryPush4(0x2D, fieldPopup->id, 6); // lcsQuery
      fieldTimeout = time + fieldPopup->valuesOffset; // + popup timeout
    }
  } else if (time > devicesRefreshTimeout && expectedFieldsCount < 1) {
    devicesRefreshTimeout = time + 100;
    crossfireTelemetryPing();
  } else if (time > fieldTimeout && expectedFieldsCount != 0/* && !edit*/) {
    if (allParamsLoaded < 1 || statusComplete == 0) {
      crossfireTelemetryPush4(0x2C, fieldId, fieldChunk);
      fieldTimeout = time + 50; // 0.5s
    }
  }

  if (time > linkstatTimeout) {
    if (!deviceIsELRS_TX && allParamsLoaded == 1) {
      goodBadPkt[0] = '\0';
    } else {
      crossfireTelemetryPush4(0x2D, 0x0, 0x0); // request linkstat
    }
    linkstatTimeout = time + 100;
  }
  if (time > titleShowWarnTimeout) {
    titleShowWarn = (elrsFlags > 3) ? !titleShowWarn : 0;
    titleShowWarnTimeout = time + 100;
  }
}

static void lcd_title() {
  lcdClear();

  const uint8_t barHeight = 9;
  if (titleShowWarn) {
    lcdDrawChar(LCD_W - FW - 1, 1, tostring(elrsFlags));
  } else {
    lcdDrawText(LCD_W - 1, 1, goodBadPkt, RIGHT);
    lcdDrawVerticalLine(LCD_W - 10, 0, barHeight, SOLID, INVERS);
  }

  if (allParamsLoaded != 1 && expectedFieldsCount > 0) {
    lcdDrawFilledRect(COL2, 0, LCD_W, barHeight, SOLID);
    luaLcdDrawGauge(0, 0, COL2, barHeight, fieldId, expectedFieldsCount); // 136b
  } else {
    lcdDrawFilledRect(0, 0, LCD_W, barHeight, SOLID);
    if (titleShowWarn) {
      lcdDrawSizedText(textXoffset, 1, elrsFlagsInfo, 16, INVERS);
    } else {
      lcdDrawSizedText(textXoffset, 1, (allParamsLoaded == 1) ? (char *)&deviceName[0] : "Loading...", DEVICE_NAME_MAX_LEN, INVERS);
    }
  }
}

static void lcd_warn() {
  lcdDrawText(textXoffset, textSize*2, "Error:");
  lcdDrawText(textXoffset, textSize*3, elrsFlagsInfo);
  lcdDrawText(LCD_W/2, textSize*5, "[OK]", BLINK + INVERS + CENTERED);
}

static void handleDevicePageEvent(event_t event) {
  if (allocatedFieldsCount == 0) {
    return;
  } else {
    if (getFieldById(backButtonId)->nameLength == 0) {
      return;
    }
  }

  if (event == EVT_VIRTUAL_EXIT) {
    if (edit) {
      edit = 0;
      FieldProps * field = getField(lineIndex);
      fieldTimeout = getTime() + 200;
      fieldId = field->id;
      fieldChunk = 0;
      fieldDataLen = 0;
      crossfireTelemetryPush4(0x2C, fieldId, fieldChunk);
    } else {
      if (folderAccess == 0 && allParamsLoaded == 1) {
        if (deviceId != 0xEE) {
          changeDeviceId(0xEE); // change device id clear expectedFieldsCount, therefore the next ping will do reloadAllField()
        } else {
          reloadAllField();
        }
        crossfireTelemetryPing();
      }
      UIbackExec();
    }
  } else if (event == EVT_VIRTUAL_ENTER) {
    if (elrsFlags > 0x1F) {
      elrsFlags = 0;
      crossfireTelemetryPush4(0x2D, 0x2E, 0x00);
    } else {
      FieldProps * field = getField(lineIndex);
      if (field != 0 && field->nameLength > 0) {
        if (field->type == TYPE_STRING) {
          ; // not implemented
        } else if (field->type < TYPE_FOLDER) {
          edit = 1 - edit;
        }
        if (!edit) {
          if (field->type < TYPE_FOLDER || field->type == TYPE_COMMAND) {
            // For editable field types and commands, request this field's
            // data again, with a short delay to allow the module EEPROM to
            // commit. Do this before save() to allow save to override
            fieldTimeout = getTime() + 20;
            fieldId = field->id;
            fieldChunk = 0;
            statusComplete = 0;
            if (field->parent) {
              // if it is inside a folder, then we reload the folder
              reloadFolder = field->parent;
            }
            fieldDataLen = 0;
          }
          functions[field->type].save(field);
        }
      }
    }
  } else if (edit) {
    if (event == EVT_VIRTUAL_NEXT) {
      incrField(1);
    } else if (event == EVT_VIRTUAL_PREV) {
      incrField(-1);
    }
  } else {
    if (event == EVT_VIRTUAL_NEXT) {
      selectField(1);
    } else if (event == EVT_VIRTUAL_PREV) {
      selectField(-1);
    }
  }
}

static void runDevicePage(event_t event) {
  handleDevicePageEvent(event);

  lcd_title();

  if (devicesLen > 1 && otherDevicesState == BTN_REQUESTED) {
    addOtherDevicesButton();
  }
  if (elrsFlags > 0x1F) {
    lcd_warn();
  } else {
    FieldProps * field;
    for (uint32_t y = 1; y < maxLineIndex+2; y++) {
      if (pageOffset+y > allocatedFieldsCount) break;
      field = getField(pageOffset+y);
      if (field == 0) {
        break;
      } else if (field->nameLength > 0) {
        uint8_t attr = (lineIndex == (pageOffset+y)) ? ((edit && BLINK) + INVERS) : 0;
        if (field->type < TYPE_FOLDER or field->type == TYPE_INFO) {
          lcdDrawSizedText(textXoffset, y*textSize+textYoffset, (char *)&namesBuffer[field->nameOffset], field->nameLength, 0);
        }
        functions[field->type].display(field, y*textSize+textYoffset, attr);
      }
    }
  }
}

static uint8_t popupCompat(event_t event) {
  showMessageBox((char *)&fieldData[POPUP_MSG_OFFSET]);
  lcdDrawText(WARNING_LINE_X, WARNING_LINE_Y+2*FH, STR_POPUPS_ENTER_EXIT);

  if (event == EVT_VIRTUAL_EXIT) {
    return RESULT_CANCEL;
  } else if (event == EVT_VIRTUAL_ENTER) {
    return RESULT_OK;
  }
  return 0;
}

static void runPopupPage(event_t event) {
  if (event == EVT_VIRTUAL_EXIT) {
    crossfireTelemetryPush4(0x2D, fieldPopup->id, 5);
    fieldTimeout = getTime() + 200;
  }

  uint8_t result = 0;
  if (fieldPopup->value == 0 && fieldPopup->valuesLength != 0) {
      popupCompat(event);
      reloadAllField();
      fieldPopup = 0;
  } else if (fieldPopup->value == 3) {
    result = popupCompat(event);
    if (fieldPopup != 0) {
      fieldPopup->valuesLength = fieldPopup->value;
    }
    if (result == RESULT_OK) {
      crossfireTelemetryPush4(0x2D, fieldPopup->id, 4);
      fieldTimeout = getTime() + fieldPopup->valuesOffset;
      fieldPopup->value = 4;
    } else if (result == RESULT_CANCEL) {
      fieldPopup = 0;
    }
  } else if (fieldPopup->value == 2) {
    result = popupCompat(event);
    if (fieldPopup != 0) {
      fieldPopup->valuesLength = fieldPopup->value;
    }
    if (result == RESULT_CANCEL) {
      crossfireTelemetryPush4(0x2D, fieldPopup->id, 5);
      fieldTimeout = getTime() + fieldPopup->valuesOffset;
      fieldPopup = 0;
    }
  }
}

void ELRSV3_stop() {
  registerCrossfireTelemetryCallback(nullptr);
  // reloadAllField();
  UIbackExec();
  fieldPopup = 0;
  deviceId = 0xEE;
  handsetId = 0xEF;

  if (cScriptRunning) {
    cScriptRunning = 0;
    memset(reusableBuffer.MSC_BOT_Data, 0, 512);
    popMenu();
  }
}

void ELRSV3_run(event_t event) {
  if (cScriptRunning == 0) {
    cScriptRunning = 1;
    expectedFieldsCount = 0;
    clearFields();
    registerCrossfireTelemetryCallback(refreshNext);
  }

  if (event == EVT_KEY_LONG(KEY_EXIT)) {
    ELRSV3_stop();
  } else { 
    if (fieldPopup != 0) {
      runPopupPage(event);
    } else {
      runDevicePage(event);
    }

    refreshNext();
  }
}
