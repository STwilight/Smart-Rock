#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h>


/* ����������� ����������� �������� */
#define On        true
#define Off       false

/* ����������� ����� ��� ����������� ��������� */
#define LCP_PIN   0		// ��������    = GPIO0
#define TMP_PIN   2		// 1-Wire ���� = GPIO2

/* ������ ��� ����������� � Wi-Fi ���� */
#define WIFI_SSID "Smart Rock"
#define WIFI_PWD  "12345678"

/* ��������� ����� ��� ������� */
#define SRV_PORT  9000

/* �������� �������� */
OneWire oneWire(TMP_PIN);
Timer procTimer;

/* ���������� ���������� ���������� */
byte addr[8];			// ����� ������� �����������
byte type_s;			// ��� ������� �����������
byte error = false;		// ���� ������ (������)
byte max_temp = 30;		// ������������ �����������
byte set_max_temp = 0;	// ���������� ������������ �����������
float temp = 0.0;		// ���������� ��� �������� �����������

void load(bool state)
{
	/* ����� ���������� ��������� */
	digitalWrite(LCP_PIN, state);
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
		delay(500);
		/* ����� software watchdog timer */
		WDT.alive();
	}

	/* �������� ������������ ������ ������ ������� */
	if(OneWire::crc8(addr, 7) != addr[7])
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
	/* ����� ��������� ����������� � ������� */
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
		if(type_s)
		{
			// ���������� � 9 ��� ��-���������
			raw = raw << 3;
			if(data[7] == 0x10)
				raw = (raw & 0xFFF0) + 12 - data[6];
		}
		else
		{
			byte cfg = (data[4] & 0x60);
			if(cfg == 0x00) raw = raw & ~7;  		// ���������� � 9 ���,  93.75 ms
			else if(cfg == 0x20) raw = raw & ~3; 	// ���������� � 10 ���, 187.5 ms
			else if(cfg == 0x40) raw = raw & ~1; 	// ���������� � 11 ���, 375 ms
			// ��-��������� ������������ ���������� � 12 ���, 750 ms
		}

		/* �������� �������������� */
		temp = (float)raw / 16.0;
	}

	return temp;
}
void printTemp(float temperature)
{
	/* ����� ������ ����������� � ������� */
	String tmp = "Temperature is ";
		   tmp.concat(temperature);
		   tmp.concat("C.");
	Serial.print(tmp);
}
void execute()
{
	/* �������� �����, ������������� �� ������� */
	temp = getTemp();

	if(max_temp != set_max_temp)
		max_temp = set_max_temp;

	if(temp < max_temp)
		load(On);
	else
		load(Off);
}

void createAP()
{
	/* ����� ��������� ������ wi-fi ������ */
	WifiStation.enable(false);
	WifiAccessPoint.enable(true);
	WifiAccessPoint.config(WIFI_SSID, WIFI_PWD, AUTH_WPA2_PSK);
	WifiAccessPoint.setIP(IPAddress(10, 0, 0, 1));
	String tmp = "AP mode enabled. SSID: \"";
		   tmp.concat(WIFI_SSID);
		   tmp.concat("\".\n");
	Serial.print(tmp);
}

void clientConnected (TcpClient* client)
{
	/* ����� ���������� � �������������� ������� */
	String tmp = "Client connected: ";
		   tmp.concat(client->getRemoteIp().toString().c_str());
		   tmp.concat(".\n");
	Serial.print(tmp);
}
bool clientReceive (TcpClient& client, char *data, int size)
{
	/* ����� ��������� ���������� ������ */
	String r_data = data;

	/* ���������� software watchdog timer ����� ������ ��������� */
	system_soft_wdt_stop();

	r_data.toLowerCase();
	if(r_data.indexOf("\n") != -1)
		r_data.replace("\n", "");
	else if(r_data.indexOf("\r") != -1)
		r_data.replace("\r", "");

	String tmp = "Data received (";
		   tmp.concat(size);
		   tmp.concat(" bytes from ");
		   tmp.concat(client.getRemoteIp().toString().c_str());
		   tmp.concat("): \"");
		   tmp.concat(r_data);
		   tmp.concat("\".\n");
	Serial.print(tmp);

	if(r_data.compareTo("status") == 0)
	{
		bool loadStatus = digitalRead(LCP_PIN);
		Serial.print("Status request. ");
		printTemp(temp);
		if(loadStatus)
			Serial.print(" Load is ON!\n");
		else
			Serial.print(" Load is OFF!\n");

		char tmpChar[7];
		dtostrf(temp, 1, 2, tmpChar);
		tmp = "temp=";
		tmp.concat(tmpChar);
		tmp.concat("\n");
		client.sendString(tmp, false);

		if(loadStatus)
			tmp = "load=on\n";
		else
			tmp = "load=off\n";
		client.sendString(tmp, false);
	}
	else if(r_data.indexOf("max_temp=") != -1)
	{
		tmp = r_data + "\n";
		r_data.replace("max_temp=", "");
		set_max_temp = (byte) r_data.toInt();
		client.sendString(tmp, false);

		tmp = "Maximum temp is set to ";
		tmp.concat(set_max_temp);
		tmp.concat("C.\n");
		Serial.print(tmp);
	}
	else
	{
		client.sendString("Unknown command!\n", false);
	}

	/* ����� software watchdog timer */
	system_soft_wdt_restart();

	return true;
}
void clientDisconnected(TcpClient& client, bool succesfull)
{
	/* ����� ���������� �� ������������� ������� */
	String tmp = "Client disconnected: ";
		   tmp.concat(client.getRemoteIp().toString().c_str());
		   tmp.concat(".\n");
	Serial.print(tmp);
}

TcpServer tcpServer(clientConnected, clientReceive, clientDisconnected);

void startTCPServer()
{
	tcpServer.listen(SRV_PORT);
	String tmp = "TCP server started. Address is ";
	       tmp.concat("10.0.0.1:");
	       tmp.concat(SRV_PORT);
	       tmp.concat(".\n");
	Serial.print(tmp);
}

void init()
{
	pinMode(LCP_PIN, OUTPUT);
	// ������������� ���� �� �����

	load(Off);
	// ���������� ��������

	Serial.systemDebugOutput(false);
	// ������ ������ ���������� ������ � UART

	Serial.begin(SERIAL_BAUD_RATE);
	// ������������� UART, �������� 115200 ��� (��-��������)

	createAP();
	// �������� � ��������� ����� �������

	startTCPServer();
	// ������ TCP-�������

	oneWire.begin();
	// ������������� ���� 1-Wire

	searchSensor();
	// ������ ��������� ������ �������

	procTimer.initializeMs(2000, execute).start();
	// ������ ������� (�������� ����)
}
