// Courtesy of ChatGPT

#include <Wire.h>

void setup()
{
	Serial.begin(9600);
	Wire.begin();
	delay(200);
}

void loop()
{
	bool found = false;
	for (byte address = 1; address < 127; ++address)
	{
		Wire.beginTransmission(address);
		if (Wire.endTransmission() != 0)
			continue;

		Serial.print(F("Found 0x"));
		if (address < 16)
			Serial.print('0');
		Serial.println(address, HEX);
		found = true;
	}
	if (!found)
		Serial.println(F("No I2C devices found"));

	delay(2000);
}
