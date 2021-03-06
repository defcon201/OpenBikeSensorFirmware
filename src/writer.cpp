/*
  Copyright (C) 2019 Zweirat
  Contact: https://openbikesensor.org

  This file is part of the OpenBikeSensor project.

  The OpenBikeSensor sensor firmware is free software: you can redistribute
  it and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  The OpenBikeSensor sensor firmware is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along with
  the OpenBikeSensor sensor firmware.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "writer.h"
#include "utils/file.h"

const String CSVFileWriter::EXTENSION = ".obsdata.csv";

int FileWriter::getTrackNumber() {
  File numberFile = SD.open("/tracknumber.txt","r");
  int trackNumber = numberFile.readString().toInt();
  numberFile.close();
  return trackNumber;
}

void FileWriter::storeTrackNumber(int trackNumber) {
  File numberFile = SD.open("/tracknumber.txt","w");
  numberFile.println(trackNumber);
  numberFile.close();
}

void FileWriter::setFileName() {
  int number = getTrackNumber();
  String base_filename = "/sensorData";
  mFileName = base_filename + String(number) + mFileExtension;
  while (SD.exists(mFileName)) {
    number++;
    mFileName = base_filename + String(number) + mFileExtension;
  }
  storeTrackNumber(number);
}

void FileWriter::correctFilename() {
  tm startTm;
  auto start = time(nullptr);
  unsigned long passedSeconds = (millis() - mStartedMillis) / 1000L;
  start -= passedSeconds;
  localtime_r(&start, &startTm);
  if (startTm.tm_year + 1900 > 2019) {
    char name[32];
    snprintf(name, sizeof (name), "/%04d-%02d-%02dT%02d.%02d.%02d-%4x",
             startTm.tm_year + 1900, startTm.tm_mon + 1, startTm.tm_mday,
             startTm.tm_hour, startTm.tm_min, startTm.tm_sec,
             (uint16_t)(ESP.getEfuseMac() >> 32));
    const String newName = name + mFileExtension;
    if(!SD.exists(mFileName.c_str()) || // already written
      SD.rename(mFileName.c_str(), newName.c_str())) {
      mFileName = newName;
      mFinalFileName = true;
    }
  }
}

uint16_t FileWriter::getBufferLength() const {
  return mBuffer.length();
}

bool FileWriter::appendString(const String &s) {
  bool stored = false;
  if (getBufferLength() < 11000) { // do not add data if our buffer is full already. We loose data here!
    mBuffer.concat(s);
    stored = true;
  }
  if (getBufferLength() > 10000 && !(digitalRead(PushButton_PIN))) {
    flush();
  }
  if (!stored && getBufferLength() < 11000) { // do not add data if our buffer is full already. We loose data here!
    mBuffer.concat(s);
    stored = true;
  }
#ifdef DEVELOP
  if (!stored) {
    Serial.printf("File buffer overflow, not allowed to write - "
                  "will skip, memory is at %dk, buffer at %u.\n",
                  ESP.getFreeHeap() / 1024, getBufferLength());
  }
#endif
  return stored;
}

bool FileWriter::flush() {
  bool result;
#ifdef DEVELOP
  Serial.printf("Writing to concrete file.\n");
#endif
  const auto start = millis();
  result = FileUtil::appendFile(SD, mFileName.c_str(), mBuffer.c_str() );
  mBuffer.clear();
  mWriteTimeMillis = millis() - start;
  if (!mFinalFileName) {
    correctFilename();
  }
#ifdef DEVELOP
  Serial.printf("Writing to concrete file done took %lums.\n", mWriteTimeMillis);
#endif
  return result;
}

unsigned long FileWriter::getWriteTimeMillis() const {
  return mWriteTimeMillis;
}

bool CSVFileWriter::writeHeader(String trackId) {
  String header;
  header += "OBSDataFormat=2&";
  header += "OBSFirmwareVersion=" + String(OBSVersion) + "&";
  header += "DeviceId=" + String((uint16_t)(ESP.getEfuseMac() >> 32), 16) + "&";
  header += "DataPerMeasurement=3&";
  header += "MaximumMeasurementsPerLine=" + String(MAX_NUMBER_MEASUREMENTS_PER_INTERVAL) + "&";
  header += "OffsetLeft=" + String(config.sensorOffsets[LEFT_SENSOR_ID]) + "&";
  header += "OffsetRight=" + String(config.sensorOffsets[RIGHT_SENSOR_ID]) + "&";
  header += "NumberOfDefinedPrivacyAreas=" + String((int) config.privacyAreas.size()) + "&";
  header += "TrackId=" + trackId + "&";
  header += "PrivacyLevelApplied=";

  String privacyString;
  if (config.privacyConfig & AbsolutePrivacy) {
    privacyString += "AbsolutePrivacy";
  }
  if (config.privacyConfig & NoPosition) {
    if (!privacyString.isEmpty()) {
      privacyString += "|";
    }
    privacyString += "NoPosition";
  }
  if (config.privacyConfig & NoPrivacy) {
    if (!privacyString.isEmpty()) {
      privacyString += "|";
    }
    privacyString += "NoPrivacy";
  }
  if (config.privacyConfig & OverridePrivacy) {
    if (!privacyString.isEmpty()) {
      privacyString += "|";
    }
    header += "OverridePrivacy";
  }
  if (privacyString.isEmpty()) {
    privacyString += "NoPrivacy";
  }
  header += privacyString + "&";
  header += "MaximumValidFlightTimeMicroseconds=" + String(MAX_DURATION_MICRO_SEC) + "&";
  header += "BluetoothEnabled=" + String(config.bluetooth) + "&";
  header += "PresetId=default&";
  header += "DistanceSensorsUsed=HC-SR04/JSN-SR04T\n";

  header += "Date;Time;Millis;Comment;Latitude;Longitude;Altitude;"
    "Course;Speed;HDOP;Satellites;BatteryLevel;Left;Right;Confirmed;Marked;Invalid;"
    "InsidePrivacyArea;Factor;Measurements";
  for (uint16_t idx = 1; idx <= MAX_NUMBER_MEASUREMENTS_PER_INTERVAL; ++idx) {
    String number = String(idx);
    header += ";Tms" + number;
    header += ";Lus" + number;
    header += ";Rus" + number;
  }
  header += "\n";
  return appendString(header);
}

bool CSVFileWriter::append(DataSet &set) {
  String csv;
  /*
    AbsolutePrivacy : When inside privacy area, the writer does noting, unless overriding is selected and the current set is confirmed
    NoPosition : When inside privacy area, the writer will replace latitude and longitude with NaNs
    NoPrivacy : Privacy areas are ignored, but the value "insidePrivacyArea" will be 1 inside
    OverridePrivacy : When selected, a full set is written, when a value was confirmed, even inside the privacy area
  */
  if (set.isInsidePrivacyArea
    && ((config.privacyConfig & AbsolutePrivacy) || ((config.privacyConfig & OverridePrivacy) && !set.confirmed))) {
    return true;
  }

  tm time;
  localtime_r(&set.time, &time);
  char date[32];
  snprintf(date, sizeof(date),
    "%02d.%02d.%04d;%02d:%02d:%02d;%u;",
    time.tm_mday, time.tm_mon + 1, time.tm_year + 1900,
    time.tm_hour, time.tm_min, time.tm_sec, set.millis);

  csv += date;
  csv += set.comment;

#ifdef DEVELOP
  if (time.tm_sec == 0) {
    csv += "DEVELOP:  GPSMessages: " + String(gps.passedChecksum())
           + " GPS crc errors: " + String(gps.failedChecksum());
  } else if (time.tm_sec == 1) {
    csv += "DEVELOP: Mem: "
           + String(ESP.getFreeHeap() / 1024) + "k Buffer: "
           + String(getBufferLength() / 1024) + "k last write time: "
           + String(getWriteTimeMillis());
  } else if (time.tm_sec == 2) {
    csv += "DEVELOP: Mem min free: "
           + String(ESP.getMinFreeHeap() / 1024) + "k";
  }
#endif
  csv += ";";

  if ((!set.location.isValid()) ||
        set.hdop.value() == 9999 ||
        set.validSatellites == 0 ||
      ((config.privacyConfig & NoPosition) && set.isInsidePrivacyArea
    && !((config.privacyConfig & OverridePrivacy) && set.confirmed))) {
    csv += ";;;;;";
  } else {
    csv += String(set.location.lat(), 6) + ";";
    csv += String(set.location.lng(), 6) + ";";
    csv += String(set.altitude.meters(), 1) + ";";
    if (set.course.isValid()) {
      csv += String(set.course.deg(), 2);
    }
    csv += ";";
    if (set.speed.isValid()) {
      csv += String(set.speed.kmph(), 2);
    }
    csv += ";";
  }
  if (set.hdop.isValid()) {
    csv += String(set.hdop.hdop(), 2);
  }
  csv += ";";
  csv += String(set.validSatellites) + ";";
  csv += String(set.batteryLevel, 2) + ";";
  if (set.sensorValues[LEFT_SENSOR_ID] < MAX_SENSOR_VALUE) {
    csv += String(set.sensorValues[LEFT_SENSOR_ID]);
  }
  csv += ";";
  if (set.sensorValues[RIGHT_SENSOR_ID] < MAX_SENSOR_VALUE) {
    csv += String(set.sensorValues[RIGHT_SENSOR_ID]);
  }
  csv += ";";
  csv += String(set.confirmed) + ";";
  csv += String(set.marked) + ";";
  csv += String(set.invalidMeasurement) + ";";
  csv += String(set.isInsidePrivacyArea) + ";";
  csv += String(set.factor) + ";";
  csv += String(set.measurements);

  for (size_t idx = 0; idx < set.measurements; ++idx) {
    csv += ";" + String(set.startOffsetMilliseconds[idx]) + ";";
    if (set.readDurationsLeftInMicroseconds[idx] > 0) {
      csv += String(set.readDurationsLeftInMicroseconds[idx]) + ";";
    } else {
      csv += ";";
    }
    if (set.readDurationsRightInMicroseconds[idx] > 0) {
      csv += String(set.readDurationsRightInMicroseconds[idx]);
    }
  }
  for (size_t idx = set.measurements; idx < MAX_NUMBER_MEASUREMENTS_PER_INTERVAL; ++idx) {
    csv += ";;;";
  }
  csv += "\n";
  return appendString(csv);
}
