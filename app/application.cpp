#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h>

/* ����������� ����������� �������� */
#define On  true
#define Off false

/* ����������� ����� ��� ����������� ��������� */
#define LCP_PIN 2 	// ��������    = GPIO2
#define TMP_PIN 4	// 1-Wire ���� = GPIO4

/* �������� �������� */
OneWire oneWire(TMP_PIN);
Timer procTimer;

/* ���������� ���������� ���������� */
byte addr[8];		// ����� ������� �����������
byte type_s;		// ��� ������� �����������
byte error = false;	// ���� ������ (������)

void load(bool state, bool msg)
{
	/* ����� ���������� ��������� */
	digitalWrite(LCP_PIN, state);

	if(msg)
	{
		if(state)
			Serial.print("Load is ON!\n");
		else
			Serial.print("Load is OFF!\n");
	}
}
void searchSensor()
{
	/* ����� ������ ������� �� ���� 1-Wire */

	/* ����� ��� �������� � ������, ���� ROM ������� ������ ������� */
	rescan:

	/* �������� ������� ������ ����� 1-Wire */
	oneWire.reset_search();

	/* ����� ��������� �� ���� 1-Wire */
	while(!oneWire.search(addr))
	{
		Serial.print("No sensors found.\n");
		oneWire.reset_search();
		delay(250);
	}

	/* �������� ������������ ������ ������ ������� */
	if (OneWire::crc8(addr, 7) != addr[7])
	{
		Serial.print("Sensor's ROM CRC is not valid!\n");
		goto rescan;
	}
	else
	{
		/* ����������� ���� ������� � ��� ����� */
		switch (addr[0])
		{
			case 0x10:
				Serial.print("Sensor is DS18S20 or DS1820.\n");
				type_s = 1;
				break;
			case 0x28:
				Serial.print("Sensor is DS18B20.\n");
				type_s = 0;
				break;
			case 0x22:
				Serial.print("Sensor is DS1822.\n");
				type_s = 0;
				break;
			default:
				Serial.print("Device is not a DS18x20 family device.\n");
				error = true;
				break;
		}

		if(!error)
		{
			/* ����� ����������� ������ ������� */
			Serial.print("Sensor's ROM is:");
			for(byte i = 0; i < 8; i++)
			{
				Serial.write(' ');
				Serial.print(addr[i], HEX);
			}

			/* ����� ������� */
			Serial.println();
		}
	}
}
float getTemp()
{
	float temp = 0;
	byte data[12];

	if(!error)
	{
		/* �������� ������� ������� �������������� (���������� �������) */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0x44, 1);

		/* ����� ��� ���������� �������� ��������� */
		delay(1000);

		/* �������� ������� ������ ����������� ������� */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0xBE);

		/* ������ ����������� */
		for (byte i = 0; i < 9; i++)
			data[i] = oneWire.read();

		/* ����������� ����������� */
		int16_t raw = (data[1] << 8) | data[0];
		if (type_s)
		{
			// ���������� � 9 ��� ��-���������
			raw = raw << 3;
			if (data[7] == 0x10)
				raw = (raw & 0xFFF0) + 12 - data[6];
		}
		else
		{
			byte cfg = (data[4] & 0x60);
			if (cfg == 0x00) raw = raw & ~7;  		// ���������� � 9 ���,  93.75 ms
			else if (cfg == 0x20) raw = raw & ~3; 	// ���������� � 10 ���, 187.5 ms
			else if (cfg == 0x40) raw = raw & ~1; 	// ���������� � 11 ���, 375 ms
			// ��-��������� ������������ ���������� � 12 ���, 750 ms
		}

		/* �������� �������������� */
		temp = (float)raw / 16.0;
	}

	return temp;
}
void printTemp(float temperature)
{
	Serial.print("Temperature is ");
	Serial.print(temperature);
	Serial.print("C.\n");
}
void execute()
{
	float temp = getTemp();

	printTemp(temp);

	if(temp < 30.0)
		load(On, true);
	else
		load(Off, true);
}

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	// ������������� UART, �������� 115200 ��� (��-��������)

	// Serial.systemDebugOutput(true);
	// ���������� ������ ���������� ������ � UART

	oneWire.begin();
	// ������������� ���� 1-Wire

	pinMode(LCP_PIN, OUTPUT);
	// ������������� ���� �� �����

	load(Off, false);
	// ���������� ��������

	searchSensor();
	// ������ ��������� ������ �������

	procTimer.initializeMs(3000, execute).start();
	// ������ ������� (����� �����������)
}
