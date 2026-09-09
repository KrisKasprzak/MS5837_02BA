/*
  MS5837_02BA.cpp - implementation. See MS5837_02BA.h for the licence and the
  revision table.
*/

#include "MS5837_02BA.h"

/* Worst-case conversion time in milliseconds, indexed by OSR value / 2.
   Datasheet maxima are 0.54, 1.06, 2.08, 4.13, 8.22 and 16.44 ms; each is
   rounded up to a whole millisecond so that delay() cannot cut one short. */
static const uint8_t MS5837_CONVERSION_MS[6] = { 1, 2, 3, 5, 9, 17 };

MS5837_02BA::MS5837_02BA()
{
  busPtr          = &Wire;
  osrSetting      = MS5837_OSR_DEFAULT;
  lastError       = MS5837_ERROR_NONE;
  promValid       = false;
  rawPressure     = 0;
  rawTemperature  = 0;
  compTemperature = 0;
  compPressure    = 0;
  fluidDensity    = MS5837_DENSITY_FRESHWATER;
  conversionStart = 0;

  for (uint8_t idx = 0; idx < MS5837_PROM_WORDS; idx++)
  {
    promWords[idx] = 0;
  }
}

bool MS5837_02BA::begin(TwoWire &busRef)
{
  busPtr = &busRef;

  if (!reset())
  {
    return false;
  }

  return readProm();
}

bool MS5837_02BA::reset(void)
{
  if (!sendCommand(MS5837_CMD_RESET))
  {
    promValid = false;
    return false;
  }

  /* The datasheet allows up to 10 ms for the PROM to reload. */
  delay(10);
  lastError = MS5837_ERROR_NONE;
  return true;
}

bool MS5837_02BA::readProm(void)
{
  promValid = false;

  for (uint8_t wordIndex = 0; wordIndex < MS5837_PROM_WORDS; wordIndex++)
  {
    if (!sendCommand(MS5837_CMD_PROM_READ + (wordIndex * 2)))
    {
      return false;
    }

    if (busPtr->requestFrom((uint8_t)MS5837_I2C_ADDRESS, (uint8_t)2) != 2)
    {
      lastError = MS5837_ERROR_BUS;
      return false;
    }

    uint16_t highByte = busPtr->read();
    uint16_t lowByte  = busPtr->read();

    promWords[wordIndex] = (highByte << 8) | lowByte;
	
  }

  if (!checkCrc())
  {
    lastError = MS5837_ERROR_CRC;
    return false;
  }

  promValid = true;
  lastError = MS5837_ERROR_NONE;
  return true;
}

bool MS5837_02BA::checkCrc(void)
{
  /* CRC-4 over the eight PROM words, per the datasheet reference routine. The
     CRC nibble itself lives in the top four bits of word 0 and is zeroed for
     the calculation; word 7 is replaced by zero. */
  uint16_t expected = (promWords[0] & 0xF000) >> 12;
  uint16_t working[MS5837_PROM_WORDS];

  for (uint8_t idx = 0; idx < MS5837_PROM_WORDS; idx++)
  {
    working[idx] = promWords[idx];	
  }
  working[0] &= 0x0FFF;
  working[7]  = 0;

  uint16_t remainder = 0;

  for (uint8_t byteIndex = 0; byteIndex < (MS5837_PROM_WORDS * 2); byteIndex++)
  {
    if ((byteIndex % 2) == 1)
    {
      remainder ^= (uint16_t)(working[byteIndex >> 1] & 0x00FF);
    }
    else
    {
      remainder ^= (uint16_t)(working[byteIndex >> 1] >> 8);
    }

    for (uint8_t bitCount = 8; bitCount > 0; bitCount--)
    {
      if (remainder & 0x8000)
      {
        remainder = (remainder << 1) ^ 0x3000;
      }
      else
      {
        remainder = (remainder << 1);
      }
    }
  }

  remainder = (remainder >> 12) & 0x000F;
  return ((remainder-1) == expected);

}

bool MS5837_02BA::setResolution(uint8_t osrValue)
{
  switch (osrValue)
  {
    case MS5837_OSR_256:
    case MS5837_OSR_512:
    case MS5837_OSR_1024:
    case MS5837_OSR_2048:
    case MS5837_OSR_4096:
    case MS5837_OSR_8192:
      osrSetting = osrValue;
      lastError  = MS5837_ERROR_NONE;
      return true;

    default:
      lastError = MS5837_ERROR_RANGE;
      return false;
  }
}

uint8_t MS5837_02BA::getResolution(void) const
{
  return osrSetting;
}

uint8_t MS5837_02BA::getConversionTime(void) const
{
  return MS5837_CONVERSION_MS[osrSetting >> 1];
}

bool MS5837_02BA::startConversion(uint8_t commandValue)
{
  if ((commandValue != MS5837_CMD_CONVERT_D1) &&
      (commandValue != MS5837_CMD_CONVERT_D2))
  {
    lastError = MS5837_ERROR_RANGE;
    return false;
  }

  if (!sendCommand(commandValue + osrSetting))
  {
    return false;
  }

  conversionStart = millis();
  return true;
}

bool MS5837_02BA::conversionDone(void) const
{
  return ((millis() - conversionStart) >= (uint32_t)getConversionTime());
}

bool MS5837_02BA::readAdc(uint32_t &resultRef)
{
  if (!sendCommand(MS5837_CMD_ADC_READ))
  {
    return false;
  }

  if (busPtr->requestFrom((uint8_t)MS5837_I2C_ADDRESS, (uint8_t)3) != 3)
  {
    lastError = MS5837_ERROR_BUS;
    return false;
  }

  uint32_t highByte = busPtr->read();
  uint32_t midByte  = busPtr->read();
  uint32_t lowByte  = busPtr->read();

  resultRef = (highByte << 16) | (midByte << 8) | lowByte;
  lastError = MS5837_ERROR_NONE;
  return true;
}

bool MS5837_02BA::read(void)
{
  if (!promValid)
  {
    lastError = MS5837_ERROR_NOT_READY;
    return false;
  }

  if (!startConversion(MS5837_CMD_CONVERT_D1))
  {
    return false;
  }

  delay(getConversionTime());

  if (!readAdc(rawPressure))
  {
    return false;
  }

  if (!startConversion(MS5837_CMD_CONVERT_D2))
  {
    return false;
  }

  delay(getConversionTime());

  if (!readAdc(rawTemperature))
  {
    return false;
  }

  compensate();
  return true;
}

void MS5837_02BA::compensate(void)
{
  /* First order compensation, MS5837-02BA datasheet. The coefficients are
     PROM words 1..6, named C1..C6 there. */
  int32_t deltaTemp = (int32_t)rawTemperature - ((int32_t)promWords[5] * 256L);

  int32_t tempValue = 2000L +
                      (int32_t)(((int64_t)deltaTemp * promWords[6]) / 8388608LL);

  int64_t offsetValue = ((int64_t)promWords[2] * 131072LL) +
                        (((int64_t)promWords[4] * deltaTemp) / 64LL);

  int64_t sensValue = ((int64_t)promWords[1] * 65536LL) +
                      (((int64_t)promWords[3] * deltaTemp) / 128LL);

  /* Second order compensation below 20 degC. */
  int64_t tempCorrection   = 0;
  int64_t offsetCorrection = 0;
  int64_t sensCorrection   = 0;

  if (tempValue < 2000L)
  {
    int64_t belowSquared = (int64_t)(tempValue - 2000L) * (tempValue - 2000L);

    tempCorrection   = (11LL * (int64_t)deltaTemp * deltaTemp) / 34359738368LL;
    offsetCorrection = (31LL * belowSquared) / 8LL;
    sensCorrection   = (63LL * belowSquared) / 32LL;
  }

  tempValue   = tempValue - (int32_t)tempCorrection;
  offsetValue = offsetValue - offsetCorrection;
  sensValue   = sensValue - sensCorrection;

  compTemperature = tempValue;
  compPressure    = (int32_t)((((int64_t)rawPressure * sensValue / 2097152LL) -
                               offsetValue) / 32768LL);
}

float MS5837_02BA::getPressure(void) const
{
  /* The 02BA reports in hundredths of a millibar. */
  return ((float)compPressure) / 100.0f;
}

float MS5837_02BA::getPressurePa(void) const
{
  return getPressure() * 100.0f;
}

float MS5837_02BA::getTemperature(void) const
{
  return ((float)compTemperature) / 100.0f;
}

float MS5837_02BA::getTemperatureF(void) const
{
  return (getTemperature() * 1.8f) + 32.0f;
}

float MS5837_02BA::getDepth(void) const
{
  /* Gauge pressure in pascal divided by rho * g. */
  return (getPressurePa() - (MS5837_SEALEVEL_MBAR * 100.0f)) /
         (fluidDensity * MS5837_GRAVITY);
}

float MS5837_02BA::getAltitude(void) const
{
  return 44330.0f *
         (1.0f - pow(getPressure() / MS5837_SEALEVEL_MBAR, 0.1902949f));
}

float MS5837_02BA::getAltitudeFt(void) const
{
  return (getAltitude() * 3.28084f);
}

void MS5837_02BA::setFluidDensity(float densityValue)
{
  fluidDensity = densityValue;
}

float MS5837_02BA::getFluidDensity(void) const
{
  return fluidDensity;
}

uint32_t MS5837_02BA::getRawPressure(void) const
{
  return rawPressure;
}

uint32_t MS5837_02BA::getRawTemperature(void) const
{
  return rawTemperature;
}

uint16_t MS5837_02BA::getPromWord(uint8_t wordIndex) const
{
  if (wordIndex >= MS5837_PROM_WORDS)
  {
    return 0;
  }

  return promWords[wordIndex];
}

uint8_t MS5837_02BA::getLastError(void) const
{
  return lastError;
}

bool MS5837_02BA::isConnected(void) const
{
  return promValid;
}

bool MS5837_02BA::sendCommand(uint8_t commandValue)
{
  busPtr->beginTransmission(MS5837_I2C_ADDRESS);
  busPtr->write(commandValue);

  if (busPtr->endTransmission() != 0)
  {
    lastError = MS5837_ERROR_BUS;
    return false;
  }

  return true;
}
