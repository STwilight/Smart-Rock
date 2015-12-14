#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h>


/* определение специальных значений */
#define On        true
#define Off       false

/* определение ножек для подключения периферии */
#define LCP_PIN   0		// нагрузка    = GPIO0
#define TMP_PIN   2		// 1-Wire шина = GPIO2

/* данные для подключения к Wi-Fi сети */
#define WIFI_SSID "Smart Rock"
#define WIFI_PWD  "12345678"

/* настройки порта для сервера */
#define SRV_PORT  9000

/* создание объектов */
OneWire oneWire(TMP_PIN);
Timer procTimer;

/* объявление глобальных переменных */
byte addr[8];			// адрес датчика температуры
byte type_s;			// тип датчика температуры
byte error = false;		// флаг ошибки (датчик)
byte max_temp = 30;		// максимальная температура
byte set_max_temp = 0;	// полученная максимальная температура
float temp = 0.0;		// переменная для хранения температуры

void load(bool state)
{
	/* метод управления нагрузкой */
	digitalWrite(LCP_PIN, state);
}
void searchSensor()
{
	/* метод поиска датчика на шине 1-Wire */
	/* метка для возврата в случае, если ROM датчика принят неверно */
	rescan:

	/* отправка команды сброса линии 1-Wire */
	oneWire.reset_search();

	/* поиск устройств на шине 1-Wire */
	while(!oneWire.search(addr))
	{
		Serial.print("No sensors found.\n");
		oneWire.reset_search();
		delay(500);
		/* сброс software watchdog timer */
		WDT.alive();
	}

	/* проверка правильности приема адреса датчика */
	if(OneWire::crc8(addr, 7) != addr[7])
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
	/* метод получения температуры с дачтика */
	float temp = 0;
	byte data[12];

	if(!error)
	{
		/* отправка комаеды запускв преобразования (паразитное питание) */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0x44, 1);

		/* пауза для выполнения датчиком измерений */
		delay(1000);

		/* отправка команды чтения температуры датчика */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0xBE);

		/* чтение температуры */
		for (byte i = 0; i < 9; i++)
			data[i] = oneWire.read();

		/* конвертация температуры */
		int16_t raw = (data[1] << 8) | data[0];
		if(type_s)
		{
			// разрешение в 9 бит по-умолчанию
			raw = raw << 3;
			if(data[7] == 0x10)
				raw = (raw & 0xFFF0) + 12 - data[6];
		}
		else
		{
			byte cfg = (data[4] & 0x60);
			if(cfg == 0x00) raw = raw & ~7;  		// разрешение в 9 бит,  93.75 ms
			else if(cfg == 0x20) raw = raw & ~3; 	// разрешение в 10 бит, 187.5 ms
			else if(cfg == 0x40) raw = raw & ~1; 	// разрешение в 11 бит, 375 ms
			// по-умолчанию используется разрешение в 12 бит, 750 ms
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
		   tmp.concat("C.");
	Serial.print(tmp);
}
void execute()
{
	/* основной метод, выполняющийся по таймеру */
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
	/* метод настройки режима wi-fi модуля */
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
	/* вывод информации о подключившемся клиенте */
	String tmp = "Client connected: ";
		   tmp.concat(client->getRemoteIp().toString().c_str());
		   tmp.concat(".\n");
	Serial.print(tmp);
}
bool clientReceive (TcpClient& client, char *data, int size)
{
	/* метод обработки полученных команд */
	String r_data = data;

	/* отключение software watchdog timer перед долгой операцией */
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

	/* сброс software watchdog timer */
	system_soft_wdt_restart();

	return true;
}
void clientDisconnected(TcpClient& client, bool succesfull)
{
	/* вывод информации об отключившемся клиенте */
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
	// инициализация пина на выход

	load(Off);
	// отключение нагрузки

	Serial.systemDebugOutput(false);
	// запрет выдачи отладочных данных в UART

	Serial.begin(SERIAL_BAUD_RATE);
	// инициализация UART, скорость 115200 бод (по-молчанию)

	createAP();
	// создание и настройка точки доступа

	startTCPServer();
	// запуск TCP-сервера

	oneWire.begin();
	// инициализация шины 1-Wire

	searchSensor();
	// запуск процедуры поиска датчика

	procTimer.initializeMs(2000, execute).start();
	// запуск таймера (основной цикл)
}
