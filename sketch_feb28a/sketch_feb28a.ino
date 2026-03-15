#include <SSD1306AsciiWire.h>

#define USING_INA3221 0
#define INA219_CALIBRATION_LEVEL 2

static_assert(INA219_CALIBRATION_LEVEL == 0 || INA219_CALIBRATION_LEVEL == 1 || INA219_CALIBRATION_LEVEL == 2, "Invalid calibration level - must be one of 0, 1 or 2");

#if USING_INA3221
#include <Adafruit_INA3221.h>

constexpr byte INA3221_I2C_ADDRESS = 0x40; // can be any of 0x40, 0x41, 0x44, and 0x45
constexpr byte INA3221_CHANNEL = 0;

// Referencing: https://manuals.plus/ae/1005005434867322
static Adafruit_INA3221 ina3221;
#else
#include <Adafruit_INA219.h>

static Adafruit_INA219 ina219;
#endif

constexpr unsigned long SAMPLE_RATE_MS = 250;

constexpr byte DISPLAY_I2C_ADDRESS = 0x3C;

static SSD1306AsciiWire display;

static unsigned long lastSample = 0;

void setup()
{
	Serial.begin(9600);
	Wire.begin();

	display.begin(&Adafruit128x32, DISPLAY_I2C_ADDRESS);
	display.clear();

#if USING_INA3221
	if (!ina3221.begin(INA3221_I2C_ADDRESS)) [[unlikely]]
	{
		display.print(F("Failed to connect to\nINA3221"));
		exit(EXIT_FAILURE);
	}
	ina3221.setAveragingMode(INA3221_AVG_16_SAMPLES);
	ina3221.setShuntResistance(INA3221_CHANNEL, 0.05);
	ina3221.setPowerValidLimits(3.0, 15.0);
#else
	if (!ina219.begin()) [[unlikely]]
	{
		display.print(F("Failed to connect to\nINA219"));
		exit(EXIT_FAILURE);
	}
#if INA219_CALIBRATION_LEVEL == 0
	ina219.setCalibration_32V_2A();
#elif INA219_CALIBRATION_LEVEL == 1
	ina219.setCalibration_32V_1A();
#elif INA219_CALIBRATION_LEVEL == 2
	ina219.setCalibration_16V_400mA();
#endif
#endif
	lastSample = millis();
}

static void printDynamic(const double number, const byte realEstate)
{
	const char numberSign = number < 0 ? '-' : ' ';
	const uint32_t absoluteWhole = abs(static_cast<int32_t>(number));

	byte wholeLength = 1; // initialise with 1 to account for sign
	if (absoluteWhole != 0)
		for (uint32_t remaining = absoluteWhole; remaining > 0; remaining /= 10)
			++wholeLength;
	else
		++wholeLength;

	if (wholeLength > realEstate) [[unlikely]]
	{
		display.clear();
		display.println(F("Cislo moc velke"));
		display.print(number, 3);
		exit(EXIT_FAILURE);
	}

	const byte remaining = realEstate - wholeLength;

	if (remaining >= 2) [[likely]]
	{
		// there is space for the fractional part
		display.print(numberSign);
		display.print(abs(number), remaining - 1); // subtract 1 to account for decimal point
	}
	else [[unlikely]]
	{
		// there is no space for the fractional part, so only print the whole part

		// no purpose printing decimal point, so left pad instead
		if (remaining == 1)
			display.print(' ');

		display.print(numberSign);
		display.print(absoluteWhole);
	}
}

void loop()
{
	static double Ah = 0.0;
	static double Wh = 0.0;

#if USING_INA3221
	const float V = ina3221.getBusVoltage(INA3221_CHANNEL);
	const float A = ina3221.getCurrentAmps(INA3221_CHANNEL);
#else
	constexpr double mA_TO_A = 1000;
	const float V = ina219.getBusVoltage_V();
	const float A = static_cast<double>(ina219.getCurrent_mA()) / mA_TO_A;
#endif
	const unsigned long now = millis();
	const double hours = static_cast<double>(now - lastSample) / 3600000.0;
	Ah += A * hours;
	Wh += A * V * hours;
	lastSample = now;

	display.setCursor(0, 0);

	display.setFont(X11fixed7x14);
	printDynamic(V, 7);
	display.setFont(X11fixed7x14B);
	display.print('V');

	display.setFont(X11fixed7x14);
	printDynamic(Ah, 8);
	display.setFont(X11fixed7x14B);
	display.println(F("Ah"));

	display.setFont(X11fixed7x14);
	printDynamic(A, 7);
	display.setFont(X11fixed7x14B);
	display.print('A');

	display.setFont(X11fixed7x14);
	printDynamic(Wh, 8);
	display.setFont(X11fixed7x14B);
	display.print(F("Wh"));

	delay(SAMPLE_RATE_MS);
}
