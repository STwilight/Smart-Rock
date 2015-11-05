#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h>


/* определение специальных значений */
#define On        true
#define Off       false

/* определение ножек дл€ подключени€ периферии */
#define LCP_PIN   2		// нагрузка    = GPIO2
#define TMP_PIN   4		// 1-Wire шина = GPIO4

/* максимальна€ температура */
#define MAX_TEMP  30.0

/* данные дл€ подключени€ к Wi-Fi сети */
#define WIFI_SSID "Smart Rock"
#define WIFI_PWD  "s1PKo1Dj"

/* настройки порта дл€ сервера */
#define SRV_PORT  9000

/* создание объектов */
OneWire oneWire(TMP_PIN);
Timer procTimer;

/* объ€вление глобальных переменных */
byte addr[8];		// адрес датчика температуры
byte type_s;		// тип датчика температуры
byte error = false;	// флаг ошибки (датчик)
float temp;

void load(bool state, bool msg)
{
	/* метод управлени€ нагрузкой */
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
	/* метод поиска датчика на шине 1-Wire */

	/* метка дл€ возврата в случае, если ROM датчика прин€т неверно */
	rescan:

	/* отправка команды сброса линии 1-Wire */
	oneWire.reset_search();

	/* поиск устройств на шине 1-Wire */
	while(!oneWire.search(addr))
	{
		Serial.print("No sensors found.\n");
		oneWire.reset_search();
		delay(250);
	}

	/* проверка правильности приема адреса датчика */
	if (OneWire::crc8(addr, 7) != addr[7])
	{
		Serial.print("Sensor's ROM CRC is not valid!\n");
		goto rescan;
	}
	else
	{
		/* определение типа датчика и его вывод */
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
			/* вывод уникального адреса датчика */
			Serial.print("Sensor's ROM is:");
			for(byte i = 0; i < 8; i++)
			{
				Serial.write(' ');
				Serial.print(addr[i], HEX);
			}

			/* вывод отступа */
			Serial.println();
		}
	}
}
float getTemp()
{
	/* метод получени€ температуры с дачтика */
	float temp = 0;
	byte data[12];

	if(!error)
	{
		/* отправка комаеды запускв преобразовани€ (паразитное питание) */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0x44, 1);

		/* пауза дл€ выполнени€ датчиком измерений */
		delay(1000);

		/* отправка команды чтени€ температуры датчика */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0xBE);

		/* чтение температуры */
		for (byte i = 0; i < 9; i++)
			data[i] = oneWire.read();

		/* конвертаци€ температуры */
		int16_t raw = (data[1] << 8) | data[0];
		if (type_s)
		{
			// разрешение в 9 бит по-умолчанию
			raw = raw << 3;
			if (data[7] == 0x10)
				raw = (raw & 0xFFF0) + 12 - data[6];
		}
		else
		{
			byte cfg = (data[4] & 0x60);
			if (cfg == 0x00) raw = raw & ~7;  		// разрешение в 9 бит,  93.75 ms
			else if (cfg == 0x20) raw = raw & ~3; 	// разрешение в 10 бит, 187.5 ms
			else if (cfg == 0x40) raw = raw & ~1; 	// разрешение в 11 бит, 375 ms
			// по-умолчанию используетс€ разрешение в 12 бит, 750 ms
		}

		/* конечное преобразование */
		temp = (float)raw / 16.0;
	}

	return temp;
}
void printTemp(float temperature)
{
	/* метод вывода темпаратуры в консоль */
	String tmp = "Temperature is ";
	tmp.concat(temperature);
	tmp.concat("C.\n");
	Serial.print(tmp);
}
void execute()
{
	/* основной метод, выполн€ющийс€ по таймеру */
	temp = getTemp();

	if(temp < MAX_TEMP)
		load(On, false);
	else
		load(Off, false);
}

void createAP()
{
	/* метод настройки режима wi-fi модул€ */
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
	String tmp = "Client connected: ";
		   tmp.concat(client->getRemoteIp().toString().c_str());
		   tmp.concat(".\n");
	Serial.print(tmp);
}
bool clientReceive (TcpClient& client, char *data, int size)
{
	String r_data = data;
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

	if (r_data.compareTo("request") == 0)
	{
		Serial.print("Temperature request. ");
		printTemp(temp);

		char tmpChar[7];
		dtostrf(temp, 1, 2, tmpChar);
		tmp = "temp=";
		tmp.concat(tmpChar);
		tmp.concat("\n");
		client.sendString(tmp, false);

		if(digitalRead(LCP_PIN))
			tmp = "load=on\n";
		else
			tmp = "load=off\n";
		client.sendString(tmp, false);
	}
	else
	{
		client.sendString("Unknown command!\n", false);
	}

	return true;
}
void clientDisconnected(TcpClient& client, bool succesfull)
{
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
	// инициализаци€ пина на выход

	load(Off, false);
	// отключение нагрузки

	Serial.systemDebugOutput(false);
	// запрет выдачи отладочных данных в UART

	Serial.begin(SERIAL_BAUD_RATE);
	// инициализаци€ UART, скорость 115200 бод (по-молчанию)

	createAP();
	// создание и настройка точки доступа

	startTCPServer();
	// запуск TCP-сервера

	oneWire.begin();
	// инициализаци€ шины 1-Wire

	searchSensor();
	// запуск процедуры поиска датчика

	procTimer.initializeMs(1500, execute).start();
	// запуск таймера (вывод температуры)
}
