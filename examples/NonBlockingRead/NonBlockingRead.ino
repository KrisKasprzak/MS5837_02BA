/*
  NonBlockingRead - drive the MS5837-02BA conversion state machine by hand so
  that loop() never blocks. Useful when the sketch also has to service a
  display, radio or logger.

  read() does the same sequence with delay(); this example splits it into
  start / poll / fetch steps.
*/

#include <Wire.h>
#include <MS5837_02BA.h>

MS5837_02BA sensor;

#define STATE_START_PRESSURE 0
#define STATE_WAIT_PRESSURE  1
#define STATE_START_TEMP     2
#define STATE_WAIT_TEMP      3

uint8_t  machineState = STATE_START_PRESSURE;
uint32_t pressureRaw  = 0;
uint32_t temperatureRaw = 0;

void setup()
{
  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(400000);

  if (!sensor.begin())
  {
    Serial.print(F("MS5837 not found, error code "));
    Serial.println(sensor.getLastError());

    while (true)
    {
      delay(1000);
    }
  }

  sensor.setResolution(MS5837_OSR_8192);
}

void loop()
{
  switch (machineState)
  {
    case STATE_START_PRESSURE:
      if (sensor.startConversion(MS5837_CMD_CONVERT_D1))
      {
        machineState = STATE_WAIT_PRESSURE;
      }
      break;

    case STATE_WAIT_PRESSURE:
      if (sensor.conversionDone() && sensor.readAdc(pressureRaw))
      {
        machineState = STATE_START_TEMP;
      }
      break;

    case STATE_START_TEMP:
      if (sensor.startConversion(MS5837_CMD_CONVERT_D2))
      {
        machineState = STATE_WAIT_TEMP;
      }
      break;

    case STATE_WAIT_TEMP:
      if (sensor.conversionDone() && sensor.readAdc(temperatureRaw))
      {
        Serial.print(F("Raw D1 "));
        Serial.print(pressureRaw);
        Serial.print(F("   Raw D2 "));
        Serial.println(temperatureRaw);

        machineState = STATE_START_PRESSURE;
      }
      break;

    default:
      machineState = STATE_START_PRESSURE;
      break;
  }

  /* other work goes here, loop() never blocks */
}
