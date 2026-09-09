/*
  MS5837_02BA.h - Arduino library for the TE Connectivity MS5837-02BA
                  miniature altimeter / barometric pressure sensor (I2C).

  ---------------------------------------------------------------------------
  MIT License

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
  ---------------------------------------------------------------------------

  Revision table

  Rev   Date      		Author        	Description
  1.0	2026-09-09 		Claude-Opus5	Initial release: Code tested and fixed by Kris Kasprzak using Teensy 4.0
  
  ---------------------------------------------------------------------------
  Device notes

  - Fixed I2C address 0x76 (the MS5837 has no address select pin).
  - Bus speed up to 400 kHz.
  - The part must be reset once after power-up so that the factory calibration
    PROM is loaded into the internal register.
  - Pressure output resolution of the 02BA variant is 0.01 mbar; temperature
    output resolution is 0.01 degC.
  - This library performs the full first- and second-order compensation given
    in the MS5837-02BA datasheet.
  ---------------------------------------------------------------------------
*/

#ifndef MS5837_02BA_H
#define MS5837_02BA_H

#include <Arduino.h>
#include <Wire.h>

/* ---------------------------------------------------------------------------
   I2C address
   --------------------------------------------------------------------------- */
#define MS5837_I2C_ADDRESS 0x76

/* ---------------------------------------------------------------------------
   Device commands
   --------------------------------------------------------------------------- */
#define MS5837_CMD_RESET      0x1E
#define MS5837_CMD_ADC_READ   0x00
#define MS5837_CMD_CONVERT_D1 0x40 /* pressure    conversion, OSR added on top */
#define MS5837_CMD_CONVERT_D2 0x50 /* temperature conversion, OSR added on top */
#define MS5837_CMD_PROM_READ  0xA0 /* base address, word index * 2 added on top */

/* ---------------------------------------------------------------------------
   Oversampling (resolution) settings.

   The value of each constant is the offset added to the conversion command,
   exactly as tabulated in the datasheet, so it can be used directly.
   --------------------------------------------------------------------------- */
#define MS5837_OSR_256  0x00 /* lowest  resolution, 0.54 ms conversion */
#define MS5837_OSR_512  0x02 /*                     1.06 ms conversion */
#define MS5837_OSR_1024 0x04 /*                     2.08 ms conversion */
#define MS5837_OSR_2048 0x06 /*                     4.13 ms conversion */
#define MS5837_OSR_4096 0x08 /*                     8.22 ms conversion */
#define MS5837_OSR_8192 0x0A /* highest resolution, 16.44 ms conversion */

#define MS5837_OSR_DEFAULT MS5837_OSR_8192

/* ---------------------------------------------------------------------------
   PROM layout: 8 x 16-bit words. Word 0 holds the factory data and the CRC-4,
   words 1..6 are the calibration coefficients, word 7 is unused.
   --------------------------------------------------------------------------- */
#define MS5837_PROM_WORDS 7

/* ---------------------------------------------------------------------------
   Error codes returned by getLastError().
   --------------------------------------------------------------------------- */
#define MS5837_ERROR_NONE      0 /* no error since the last operation          */
#define MS5837_ERROR_BUS       1 /* the device did not acknowledge on the bus  */
#define MS5837_ERROR_CRC       2 /* the PROM CRC-4 check failed                */
#define MS5837_ERROR_RANGE     3 /* an argument was outside its allowed range  */
#define MS5837_ERROR_NOT_READY 4 /* read attempted before a successful begin() */

/* ---------------------------------------------------------------------------
   Physical constants used by the derived readings.
   --------------------------------------------------------------------------- */
#define MS5837_DENSITY_FRESHWATER 997.0f  /* kg/m^3                            */
#define MS5837_DENSITY_SEAWATER   1029.0f /* kg/m^3                            */
#define MS5837_GRAVITY            9.80665f /* m/s^2                            */
#define MS5837_SEALEVEL_MBAR      1013.25f /* standard atmosphere, mbar        */

class MS5837_02BA
{
  public:

    /* Construct the driver. The sensor is not touched until begin() runs. */
    MS5837_02BA();

    /* Reset the part, read and CRC-check the PROM. Returns true on success.
       Pass an alternative TwoWire instance on boards with several I2C ports.
       Wire.begin() is the sketch's responsibility, not the library's. */
    bool begin(TwoWire &busRef = Wire);

    /* Issue the RESET command and wait for the PROM to reload. */
    bool reset(void);

    /* Re-read the eight PROM words and verify the CRC-4. */
    bool readProm(void);

    /* Verify the CRC-4 nibble held in PROM word 0 against words 0..6. */
    bool checkCrc(void);

    /* Set the oversampling ratio used by every later conversion.
       Accepts one of the MS5837_OSR_* manifest constants. Returns false and
       leaves the setting unchanged if the argument is not one of them. */
    bool setResolution(uint8_t osrValue);

    /* The oversampling ratio currently in force, as an MS5837_OSR_* value. */
    uint8_t getResolution(void) const;

    /* Worst-case conversion time in milliseconds for the current resolution. */
    uint8_t getConversionTime(void) const;

    /* Run one pressure and one temperature conversion and compensate both.
       Returns true when fresh values are available from the accessors. */
    bool read(void);

    /* Start a single conversion without blocking. Use with conversionDone()
       and readAdc() when the sketch cannot afford to wait inside read().
       commandValue is MS5837_CMD_CONVERT_D1 or MS5837_CMD_CONVERT_D2. */
    bool startConversion(uint8_t commandValue);

    /* True once getConversionTime() milliseconds have elapsed since the last
       startConversion() call. */
    bool conversionDone(void) const;

    /* Read the 24-bit result of the last conversion into resultRef. */
    bool readAdc(uint32_t &resultRef);

    /* Compensated readings from the most recent successful read(). */
    float getPressure(void) const;                     /* mbar (hPa)          */
    float getPressurePa(void) const;                   /* pascal              */
    float getTemperature(void) const;                  /* degrees celsius     */
    float getTemperatureF(void) const;                 /* degrees fahrenheit  */

    /* Depth below the surface, using the density set by setFluidDensity(). */
    float getDepth(void) const;                        /* metres              */

    /* Altitude above the standard-atmosphere sea level datum. */
    float getAltitude(void) const;                     /* metres              */
	float getAltitudeFt(void) const;                     /* feet              */

    /* Fluid density used by getDepth(). Defaults to fresh water. */
    void setFluidDensity(float densityValue);
    float getFluidDensity(void) const;

    /* Raw uncompensated conversion results from the most recent read(). */
    uint32_t getRawPressure(void) const;
    uint32_t getRawTemperature(void) const;

    /* One PROM word, 0..7. Returns 0 when wordIndex is out of range. */
    uint16_t getPromWord(uint8_t wordIndex) const;

    /* The error recorded by the most recent operation, MS5837_ERROR_*. */
    uint8_t getLastError(void) const;

    /* True once begin() has completed with a valid PROM. */
    bool isConnected(void) const;

  private:

    bool sendCommand(uint8_t commandValue);
    void compensate(void);

    TwoWire *busPtr;
    uint16_t promWords[MS5837_PROM_WORDS];
    uint8_t  osrSetting;
    uint8_t  lastError;
    bool     promValid;

    uint32_t rawPressure;
    uint32_t rawTemperature;
    int32_t  compTemperature; /* hundredths of a degree celsius */
    int32_t  compPressure;    /* hundredths of a millibar       */

    float    fluidDensity;
    uint32_t conversionStart; /* millis() at the last startConversion() */
};

#endif /* MS5837_02BA_H */
