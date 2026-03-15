#include <Arduino.h>

// SDA/A4, SCL/A5
#include <SSD1306AsciiWire.h>

constexpr uint8_t DISPLAY_I2C_ADDRESS = 0x3C;

constexpr uint8_t STEPS_PER_REVOLUTION = 200;
constexpr uint8_t MICROSTEP_MULTIPLIER = 8;
constexpr uint16_t MICROSTEPS_PER_REVOLUTION = STEPS_PER_REVOLUTION * MICROSTEP_MULTIPLIER;

enum pin
{
	PIN_CLK = 2,
	PIN_DT,
	PIN_SW,
	PIN_DIR,
	PIN_STEP,
	PIN_ENABLE,
	PIN_MS3,
	PIN_MS2,
	PIN_MS1
};

enum direction
{
	// these helpfully correspond to the rotation of both the motor and the rotary encoder
	DIRECTION_COUNTERCLOCKWISE = LOW,
	DIRECTION_CLOCKWISE = HIGH,
};

enum button_pressed
{
	BUTTON_PRESSED_TRUE = LOW,
	BUTTON_PRESSED_FALSE = HIGH,
};

enum step
{
	STEP_OFF = LOW,
	STEP_ON = HIGH,
};

enum motor_enabled
{
	// wtf????? why is the enable value 0?
	MOTOR_ENABLED = LOW,
	MOTOR_DISABLED = HIGH,
};

static_assert(MICROSTEP_MULTIPLIER == 1 || MICROSTEP_MULTIPLIER == 2 || MICROSTEP_MULTIPLIER == 4 || MICROSTEP_MULTIPLIER == 8 || MICROSTEP_MULTIPLIER == 16, "Invalid microstep multiplier - allowed values are 1, 2, 4, 8 or 16");

SSD1306AsciiWire display;

volatile int lastClk;
volatile enum direction lastDirection = DIRECTION_CLOCKWISE;

volatile byte edges = 2;
volatile bool edgesCommitted = false;
volatile int startingPosition = 0;
volatile bool startingPositionCommitted = false;
volatile byte edge = 1;

static void step()
{
	constexpr unsigned int PULSE_LENGTH_USECS = 1000;
	digitalWrite(PIN_STEP, STEP_ON);
	delayMicroseconds(PULSE_LENGTH_USECS);
	digitalWrite(PIN_STEP, STEP_OFF);
}

static void readEncoder()
{
	const int clk = digitalRead(PIN_CLK);
	if (clk == lastClk)
		return;
	lastClk = clk;
	lastDirection = static_cast<enum direction>(digitalRead(PIN_DT) != clk);
	const int8_t delta = lastDirection == DIRECTION_CLOCKWISE ? 1 : -1;
	if (!edgesCommitted)
		edges = constrain(edges + delta, 2, 12);
	else if (!startingPositionCommitted)
	{
		digitalWrite(PIN_DIR, lastDirection);
		step();
		startingPosition += delta;
	}
	else
	{
		edge += delta;
		if (edge == 0)
			edge = edges;
		else if (edge > edges)
			edge = 1;
	}
}

void setup()
{
	Serial.begin(9600);
	Wire.begin();

	// display
	display.begin(&Adafruit128x32, DISPLAY_I2C_ADDRESS);
	display.setFont(TimesNewRoman16);
	display.setCursor(0, 1);
	display.print(F("Polohy: "));
	display.setCol(64);
	display.print('2');

	// encoder
	pinMode(PIN_CLK, INPUT_PULLUP);
	lastClk = digitalRead(PIN_CLK);
	attachInterrupt(digitalPinToInterrupt(PIN_CLK), readEncoder, CHANGE);
	pinMode(PIN_DT, INPUT_PULLUP);
	pinMode(PIN_SW, INPUT_PULLUP);

	// motor
	pinMode(PIN_ENABLE, OUTPUT);
	digitalWrite(PIN_ENABLE, MOTOR_ENABLED);
	pinMode(PIN_STEP, OUTPUT);
	pinMode(PIN_DIR, OUTPUT);
	pinMode(PIN_MS1, OUTPUT);
	pinMode(PIN_MS2, OUTPUT);
	pinMode(PIN_MS3, OUTPUT);
	switch (MICROSTEP_MULTIPLIER)
	{
	case 1:
		digitalWrite(PIN_MS1, LOW);
		digitalWrite(PIN_MS2, LOW);
		digitalWrite(PIN_MS3, LOW);
		break;
	case 2:
		digitalWrite(PIN_MS1, HIGH);
		digitalWrite(PIN_MS2, LOW);
		digitalWrite(PIN_MS3, LOW);
		break;
	case 4:
		digitalWrite(PIN_MS1, LOW);
		digitalWrite(PIN_MS2, HIGH);
		digitalWrite(PIN_MS3, LOW);
		break;
	case 8:
		digitalWrite(PIN_MS1, HIGH);
		digitalWrite(PIN_MS2, HIGH);
		digitalWrite(PIN_MS3, LOW);
		break;
	case 16:
		digitalWrite(PIN_MS1, HIGH);
		digitalWrite(PIN_MS2, HIGH);
		digitalWrite(PIN_MS3, HIGH);
		break;
	default:
		display.print(F("Invalid microstep\nmultiplier"));
		exit(EXIT_FAILURE);
	}
}

void loop()
{
	static bool pressed = digitalRead(PIN_SW) == BUTTON_PRESSED_TRUE;
	static bool lastPressed = digitalRead(PIN_SW) == BUTTON_PRESSED_TRUE;
	static unsigned long lastSwitchChanged = 0;

	static byte lastEdges = 1;
	static int lastStartingPosition = 0;
	static byte lastEdge = 1;
	static int position = 0;

	bool raw = digitalRead(PIN_SW) == BUTTON_PRESSED_TRUE;
	const unsigned long now = millis();
	if (raw != pressed)
	{
		pressed = raw;
		lastSwitchChanged = now;
	}

	if (now - lastSwitchChanged > 50 && pressed != lastPressed)
	{
		lastPressed = pressed;

		if (!pressed)
			return;

		display.clear();

		if (!edgesCommitted)
		{
			edgesCommitted = true;
			display.println(F("Nastavenie 0 pozicii"));
			display.print(startingPosition);
		}
		else if (!startingPositionCommitted)
		{
			startingPositionCommitted = true;
			display.setRow(1);
			display.print(F("Poloha: "));
			display.setCol(64);
			display.print(edge);
			display.print(F(" / "));
			display.print(edges);
		}
		else
		{
			edges = 2;
			edgesCommitted = false;
			startingPosition = 0;
			startingPositionCommitted = false;
			lastEdges = 1;
			lastEdge = 1;
			edge = 1;
			position = 0;
			display.setCursor(0, 1);
			display.print(F("Polohy: "));
			display.setCol(64);
			display.print(edges);
		}
	}

	if (!edgesCommitted)
	{
		if (edges == lastEdges)
			return;
		lastEdges = edges;
		display.setCursor(64, 1);
		display.print(edges);
		display.print(F("  "));
		return;
	}

	if (!startingPositionCommitted)
	{
		if (startingPosition == lastStartingPosition)
			return;
		lastStartingPosition = startingPosition;
		display.setCursor(0, 2);
		display.print(startingPosition);
		display.print(F("  "));
		return;
	}

	if (lastEdge == edge)
		return;

	lastEdge = edge;
	display.setCursor(64, 1);
	display.print(edge);
	display.print(F(" / "));
	display.print(edges);
	display.print(F("  "));

	// todo apparently this bad
	int newPosition = round(MICROSTEPS_PER_REVOLUTION * (static_cast<double>(edge - 1) / static_cast<double>(edges)));
	if (newPosition < 0)
		newPosition += MICROSTEPS_PER_REVOLUTION * (newPosition / MICROSTEPS_PER_REVOLUTION);
	else if (newPosition >= MICROSTEPS_PER_REVOLUTION)
		newPosition -= MICROSTEPS_PER_REVOLUTION * (newPosition / MICROSTEPS_PER_REVOLUTION);

	digitalWrite(PIN_DIR, lastDirection);

	int ahead, behind;
	if (lastDirection == DIRECTION_CLOCKWISE)
	{
		ahead = newPosition;
		behind = position;
	}
	else
	{
		ahead = position;
		behind = newPosition;
	}
	int delta = ahead - behind;
	if (delta < 0)
		delta = (ahead + MICROSTEPS_PER_REVOLUTION) - behind;
	for (unsigned int i = 0; i < abs(delta); ++i)
	{
		step();
		delayMicroseconds(5);
	}

	position = newPosition;
}