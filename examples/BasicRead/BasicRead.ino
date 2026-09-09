/*
  BasicRead - blocking read of pressure and temperature from an MS5837-02BA.

  Wiring: SDA -> SDA, SCL -> SCL, VDD -> 3.3 V, GND -> GND.
  The MS5837 is a 3.3 V part; use level shifting on a 5 V board.
*/

#include <Wire.h>
#include <MS5837_02BA.h>

MS5837_02BA Altimiter;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    ; /* wait for the USB serial port on native-USB boards */
  }

  Wire.begin();
  Wire.setClock(400000);

  if (!Altimiter.begin())
  {
    Serial.print(F("MS5837 not found, error code "));
    Serial.println(Altimiter.getLastError());

    while (true)
    {
      delay(1000);
    }
  }

  /* Trade conversion time for noise: MS5837_OSR_256 is fastest,
     MS5837_OSR_8192 is quietest. */
  Altimiter.setResolution(MS5837_OSR_8192);

  Serial.print(F("Conversion time per sample, ms: "));
  Serial.println(Altimiter.getConversionTime());
}

void loop()
{
  if (Altimiter.read())
  {
    //Serial.print(F("Pressure "));
    //Serial.print(Altimiter.getPressure(), 2);
    //Serial.print(F(" mbar   Temperature "));
    //Serial.print(Altimiter.getTemperatureF(), 2);
    //Serial.print(F(" F   Altitude "));
    Serial.print("500,550,");
    Serial.print(Altimiter.getAltitudeFt(), 3);
    Serial.println();
  }
  else
  {
    Serial.print(F("Read failed, error code "));
    Serial.println(Altimiter.getLastError());
  }

  delay(1000);
}
