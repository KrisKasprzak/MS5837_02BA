# MS5837_02BA

Arduino library for the TE Connectivity **MS5837-02BA** barometric pressure and
temperature sensor (I2C, fixed address `0x76`, 3.3 V). Note this code was written by ClaudeCode-Opus5 on 9/9/2026. Some minor bugs were found and fixed. Tested on a Teensy 4.0.

MIT licensed — the full licence text and the revision table are at the top of
`MS5837_02BA.h`.

## Install

Copy the `MS5837_02BA` folder into your Arduino `libraries` directory, then
restart the IDE.

## Quick start

```cpp
#include <Wire.h>
#include <MS5837_02BA.h>

MS5837_02BA sensor;

void setup()
{
  Wire.begin();
  sensor.begin();
  sensor.setResolution(MS5837_OSR_4096);
}

void loop()
{
  if (sensor.read())
  {
    float pressureMbar = sensor.getPressure();
    float temperatureC = sensor.getTemperature();
  }
  delay(1000);
}
```

`Wire.begin()` is the sketch's job, not the library's, so a board with several
I2C ports can pass its own bus: `sensor.begin(Wire1)`.

## Resolution

`setResolution()` takes one of the manifest constants below and returns `false`
without changing anything if given something else. Times are the datasheet
maxima; `getConversionTime()` returns the same figure rounded up to whole
milliseconds.

| Constant          | Oversampling | Conversion time |
|-------------------|--------------|-----------------|
| `MS5837_OSR_256`  | 256          | 0.54 ms         |
| `MS5837_OSR_512`  | 512          | 1.06 ms         |
| `MS5837_OSR_1024` | 1024         | 2.08 ms         |
| `MS5837_OSR_2048` | 2048         | 4.13 ms         |
| `MS5837_OSR_4096` | 4096         | 8.22 ms         |
| `MS5837_OSR_8192` | 8192         | 16.44 ms        |

`MS5837_OSR_DEFAULT` is `MS5837_OSR_8192`, which is what a freshly constructed
object uses. A `read()` runs two conversions, so a full sample costs twice the
figure in the table.

## API

**Setup and device control**

| Method | Purpose |
|---|---|
| `begin(busRef = Wire)` | Reset, read the PROM, verify its CRC-4 |
| `reset()` | Issue `RESET` and wait 10 ms for the PROM to reload |
| `readProm()` | Re-read the eight PROM words and CRC-check them |
| `checkCrc()` | Verify the CRC-4 nibble against words 0..6 |
| `isConnected()` | True once `begin()` succeeded |
| `getLastError()` | `MS5837_ERROR_*` code from the most recent operation |

**Resolution**

`setResolution(osrValue)`, `getResolution()`, `getConversionTime()`.

**Measurement**

| Method | Purpose |
|---|---|
| `read()` | Blocking: both conversions plus full compensation |
| `startConversion(cmd)` | Non-blocking start, `MS5837_CMD_CONVERT_D1` or `_D2` |
| `conversionDone()` | True once the conversion time has elapsed |
| `readAdc(resultRef)` | Fetch the 24-bit result of the last conversion |

**Results from the most recent `read()`**

`getPressure()` (mbar), `getPressurePa()` (Pa), `getTemperature()` (°C),
`getTemperatureF()` (°F), `getDepth()` (m), `getAltitude()` (m),
`getRawPressure()`, `getRawTemperature()`, `getPromWord(wordIndex)`.

**Depth**

`getDepth()` uses the density set by `setFluidDensity()`, which defaults to
`MS5837_DENSITY_FRESHWATER`; `MS5837_DENSITY_SEAWATER` is also provided. It is
referenced to the standard sea-level atmosphere, not to the pressure measured
at your surface, so calibrate against a surface reading if you need better than
a few tens of centimetres.

## Compensation

`read()` applies the first-order compensation and the below-20 °C second-order
correction from the MS5837-02BA datasheet, using 64-bit intermediates. Output
resolution is 0.01 mbar and 0.01 °C.

The non-blocking path (`startConversion` / `conversionDone` / `readAdc`) hands
back **raw** ADC counts only — compensation is internal to `read()`. Use the
non-blocking calls when you want the raw stream or your own filtering, and
`read()` when you want compensated engineering units.

## Notes

- Fixed address `0x76`; the part has no address select pin, so one sensor per
  bus unless you multiplex.
- Bus speed up to 400 kHz.
- 3.3 V part — level shift on a 5 V board.
- Errors are sticky only until the next operation; check `getLastError()`
  immediately after a call returns `false`.
