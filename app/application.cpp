#include <user_config.h>
#include <settings.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h>


/* версия программного обеспечения модуля */
#define FIRMWARE	 "1.0 Alpha"

/* определение специальных значений */
#define On			 true
#define Off			 false

/* определение ножек для подключения периферии */
#define LCP_PIN		 0		// нагрузка    = GPIO0
#define TMP_PIN		 2		// 1-Wire шина = GPIO2

/* настройки таймеров отсчета времени */
uint64_t uptime	   = 0;		// время работы системы (в миллисекундах)
uint64_t heater_on = 0;		// время работы обогревателя (в миллисекундах)
#define PERIOD		 1000	// период обновления данных (в миллисекундах)

/* настройки для создания точки доступа Wi-Fi */
String ap_wifi_ssid  = "Smart Rock";
String ap_wifi_pwd   = "12345678";
bool   ap_wifi		 = On;

/* настройки для подключения к Wi-Fi сети */
/* если поля не заполнены, то данные берутся из переменных IDE */
String st_wifi_ssid = "";
String st_wifi_pwd  = "";
bool   st_wifi		= On;

/* настройки FTP-сервера */
/* если поля логина и пароля не заполнены, то данные (логин и пароль)
 * будут совпадать с данныеми для режима точки доступа */
#define FTP_SRV_NAME "Smart Rock"
#define FTP_SRV_PASS "12345678"
#define FTP_SRV_PORT 21

/* настройки порта для WEB-сервера */
#define WEB_SRV_PORT 80

/* настройки порта для TCP-сервера */
#define TCP_SRV_PORT 9000

/* настройка вывода информации в консоль */
#define UART_ENABLE	 Off
#define UART_MSG	 Off
#define UART_DEBUG	 Off
#define UART_SPEED	 115200

/* создание объектов */
OneWire oneWire(TMP_PIN);
Timer executeTimer, timeCounter, autoSaver;
HttpServer webServer;
FTPServer ftpServer;

/* объявление глобальных переменных */
byte addr[8];				// адрес датчика температуры
byte type_s;				// тип датчика температуры
byte sensor_error = false;	// флаг ошибки (датчик)
byte max_temp = 0;			// максимальная температура
byte set_max_temp = 0;		// полученная максимальная температура
float temp = 0.0;			// переменная для хранения значения измеренной температуры
bool heater_status = false;	// переменная для хранения состояния обогревателя (включен/выключен)

void load(bool state)
{
	/* метод управления нагрузкой */
	digitalWrite(LCP_PIN, state);
}

void searchSensor()
{
	/* метод поиска датчика на шине 1-Wire */
	/* счетчик попыток чтения адреса датчика */
	byte scan_attempt = 0;

	/* метка для возврата в случае, если ROM датчика принят неверно */
	rescan:

	/* отправка команды сброса линии 1-Wire */
	oneWire.reset_search();

	/* поиск устройств на шине 1-Wire */
	while(!oneWire.search(addr))
	{
		if(scan_attempt >= 10) {
			sensor_error = true;
			if(UART_ENABLE)
				Serial.print("Sensor not found!\n");
			break;
		}
		if(UART_ENABLE)
			Serial.print("No sensors found!\n");
		oneWire.reset_search();
		scan_attempt++;
		WDT.alive();
		delay(500);
	}

	/* проверка правильности приема адреса датчика */
	if(OneWire::crc8(addr, 7) != addr[7])
	{
		if(UART_ENABLE)
			Serial.print("Sensor's ROM CRC is not valid!\n");
		if(!sensor_error) {
			scan_attempt = 0;
			goto rescan;
		}
	}
	else
	{
		if(!sensor_error)
		{
			/* определение типа датчика и его вывод */
			switch (addr[0])
			{
				case 0x10:
					if(UART_ENABLE)
						Serial.print("Sensor is DS18S20 or DS1820.\n");
					type_s = 1;
					break;
				case 0x28:
					if(UART_ENABLE)
						Serial.print("Sensor is DS18B20.\n");
					type_s = 0;
					break;
				case 0x22:
					if(UART_ENABLE)
						Serial.print("Sensor is DS1822.\n");
					type_s = 0;
					break;
				default:
					if(UART_ENABLE)
						Serial.print("Device is not a DS18x20 family device.\n");
					break;
			}

			/* вывод уникального адреса датчика */
			if(UART_ENABLE) {
				Serial.print("Sensor's ROM is:");
				for(byte i = 0; i < 8; i++)
				{
					Serial.write(' ');
					Serial.print(addr[i], HEX);
				}
				/* вывод отступа */
				Serial.println();
			}

			/* смена разрешения датчика */
			/* 12 бит (750 мс,   0.0625): RES = 0x7F
			 * 11 бит (375 мс,   0.125):  RES = 0x5F
			 * 10 бит (187.5 мс, 0.25):   RES = 0x3F
			 *  9 бит (93.75 мс, 0.5):    RES = 0x1F */
		    WDT.enable(false);
			oneWire.reset();
		    oneWire.select(addr);
		    oneWire.write(0x4E);
		    oneWire.write(0x00);
		    oneWire.write(0x00);
		    oneWire.write(0x5F);
		    oneWire.reset();
		    WDT.alive();
		}
	}
}
float getTemp()
{
	/* метод получения температуры с дачтика */
	float temp = 0;
	byte data[12];

	if(!sensor_error)
	{
		/* отправка команды запускв преобразования (паразитное питание) */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0x44, 1);

		/* пауза для выполнения датчиком измерений */
		WDT.alive();
		delay(450);

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

void timeCount()
{
	/* метод обновления значений счетчиков времени работы системы и нагревателя */
	uptime += PERIOD;

	heater_status = digitalRead(LCP_PIN);
	if(heater_status)
		heater_on += PERIOD;
}
void execute()
{
	/* метод-исполнитель, выполняющийся по таймеру */
	temp = getTemp();

	if(max_temp != set_max_temp)
		max_temp = set_max_temp;

	if((temp < max_temp) & !sensor_error)
		load(On);
	else
		load(Off);

	timeCount();
}
void autoSave()
{
	/* метод для автоматического периодического сохранения данных */
	Settings.heater_on = heater_on;
	Settings.save();
}

String convertTime(uint64_t time_ms, bool include_ms)
{
	uint64_t time = time_ms;

	String days;
	String hours;
	String minutes;
	String seconds;
	String miliseconds;

	String result;
	char buffer[20];

	if(include_ms) {
		sprintf(buffer, "%03u", time%1000);
		miliseconds = buffer;
	}

	time /= 1000;
	sprintf(buffer, "%02u", time%60);
	seconds = buffer;

	time /= 60;
	sprintf(buffer, "%02u", time%60);
	minutes = buffer;

	time /= 60;
	sprintf(buffer, "%02u", time%24);
	hours = buffer;

	sprintf(buffer, "%u", time/24);
	days = buffer;

	if(!days.equals("0"))
		result = days + " day(s), ";
	result += hours + ":" + minutes + ":" + seconds;
	if(include_ms)
		result += "." + miliseconds;

	return result;
}
String convertMAC(String macAddress)
{
	String result = macAddress;
	result.toUpperCase();

	char macChar[macAddress.length() + 1];
	result.toCharArray(macChar, macAddress.length() + 1);

	result = "";
	byte size = sizeof(macChar) - 1;
	for(int i=0; i<size; i++) {
		result += macChar[i];
		if((i%2 != 0) & (i != size-1))
			result += ":";
	}

	return result;
}
String convertSN(String macAddress)
{
	String tmp;
	//long result;

	char macChar[macAddress.length() + 1];
	macAddress.toCharArray(macChar, macAddress.length() + 1);

	byte size = sizeof(macChar) - 1;
	for(int i=6; i<size; i++)
		tmp += macChar[i];

	//sscanf(tmp.c_str(), "%x", &result);
	//return result;

	/* необходимо использовать конвертацию из String в uint32_t
	 * посредством использования недоступной сейчас в Sming функции sscanf() */

	return tmp;
}

void uartInit()
{
	/* метод настройки UART модуля */
	Serial.systemDebugOutput(UART_DEBUG);

	if(UART_ENABLE) {
		Serial.begin(UART_SPEED);
	}
}
void wifiInit()
{
	/* метод настройки режима Wi-Fi модуля */
	/* проверка на наличие измененной конфигурации для режима станции */
	if((st_wifi_ssid.length() == 0) or (st_wifi_pwd.length() == 0)) {
		st_wifi_ssid = WIFI_SSID;
		st_wifi_pwd	 = WIFI_PWD;
	}

	/* настройка  */
	WifiStation.enable(st_wifi);
	if(st_wifi) {
		WifiStation.config(st_wifi_ssid, st_wifi_pwd);
		if(UART_MSG) {
			String tmp = "Client mode enabled. SSID: \"";
				   tmp.concat(st_wifi_ssid);
				   tmp.concat("\".\n");
			Serial.print(tmp);
		}
	}

	WifiAccessPoint.enable(ap_wifi);
	if(ap_wifi) {
		WifiAccessPoint.config(ap_wifi_ssid, ap_wifi_pwd, AUTH_WPA2_PSK);
		WifiAccessPoint.setIP(IPAddress(10, 0, 0, 1));
		if(UART_MSG) {
			String tmp = "AP mode enabled. SSID: \"";
				   tmp.concat(ap_wifi_ssid);
				   tmp.concat("\".\n");
			Serial.print(tmp);
		}
	}
}
void wifiReInit()
{
	/* метод перенастройки режима Wi-Fi модуля */
	Settings.load();

	if(Settings.exist()) {
		if((ap_wifi != Settings.ap_mode) or (ap_wifi_ssid != Settings.ap_ssid) or (ap_wifi_pwd != Settings.ap_psw)) {
			/*
				ap_wifi = Settings.ap_mode;
				ap_wifi_ssid = Settings.ap_ssid;
				ap_wifi_pwd = Settings.ap_psw;
				WDT.enable(false);
				WifiAccessPoint.enable(ap_wifi);
				if(ap_wifi) {
					WifiAccessPoint.config(ap_wifi_ssid, ap_wifi_pwd, AUTH_WPA2_PSK);
					WifiAccessPoint.setIP(IPAddress(10, 0, 0, 1));
				}
				WDT.alive();
			*/

			/* IP адрес адекватно назначается лишь в случае конфигурации после запуска.
			 * Временная мера – перезагрузка системы. */

			executeTimer.stop();
			autoSaver.stop();
			spiffs_unmount();
			System.restart();
		}

		if((st_wifi_ssid != Settings.st_ssid) or (st_wifi_pwd != Settings.st_psw)) {
			st_wifi_ssid = Settings.st_ssid;
			st_wifi_pwd = Settings.st_psw;
			WDT.enable(false);
			WifiStation.disconnect();
			WifiStation.config(Settings.st_ssid, Settings.st_psw);
			WDT.alive();
		}
	}
}

void clientConnected (TcpClient* client)
{
	/* вывод информации о подключившемся клиенте */
	if(UART_MSG) {
		String tmp = "Client connected: ";
			   tmp.concat(client->getRemoteIp().toString().c_str());
			   tmp.concat(".\n");
		Serial.print(tmp);
	}
}
bool clientReceive (TcpClient& client, char *data, int size)
{
	/* метод обработки полученных команд */
	String r_data = data;
	String tmp;

	/* отключение software watchdog timer перед долгой операцией */
	//system_soft_wdt_stop();
	WDT.enable(false);

	r_data.toLowerCase();
	if(r_data.indexOf("\n") != -1)
		r_data.replace("\n", "");
	else if(r_data.indexOf("\r") != -1)
		r_data.replace("\r", "");

	if(UART_MSG) {
		tmp = "Data received (";
		tmp.concat(size);
		tmp.concat(" bytes from ");
		tmp.concat(client.getRemoteIp().toString().c_str());
		tmp.concat("): \"");
		tmp.concat(r_data);
		tmp.concat("\".\n");
		Serial.print(tmp);
	}

	if(r_data.compareTo("status") == 0)
	{
		bool loadStatus = heater_status;

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

		if(UART_MSG) {
			Serial.print("Status request. ");
				printTemp(temp);
			if(loadStatus)
				Serial.print(" Load is ON!\n");
			else
				Serial.print(" Load is OFF!\n");
		}
	}
	else if(r_data.indexOf("max_temp=") != -1)
	{
		tmp = r_data + "\n";
		r_data.replace("max_temp=", "");
		set_max_temp = (byte) r_data.toInt();
		client.sendString(tmp, false);

		if(UART_MSG) {
			tmp = "Maximum temp is set to ";
			tmp.concat(set_max_temp);
			tmp.concat("C.\n");
			Serial.print(tmp);
		}
	}
	else
	{
		client.sendString("Unknown command!\n", false);
	}

	/* сброс software watchdog timer */
	//system_soft_wdt_restart();
	WDT.alive();

	return true;
}
void clientDisconnected(TcpClient& client, bool succesfull)
{
	/* вывод информации об отключившемся клиенте */
	if(UART_MSG) {
		String tmp = "Client disconnected: ";
			   tmp.concat(client.getRemoteIp().toString().c_str());
			   tmp.concat(".\n");
		Serial.print(tmp);
	}
}

TcpServer tcpServer(clientConnected, clientReceive, clientDisconnected);

void startTCPServer()
{
	tcpServer.listen(TCP_SRV_PORT);
	if(UART_MSG) {
		String tmp = "TCP server started on port ";
			   tmp.concat(TCP_SRV_PORT);
			   tmp.concat(".\n");
		Serial.print(tmp);
	}
}
void startFTPServer()
{
	if(!fileExist("status.html"))
		fileSetContent("status.html", "404 Not Found");

	ftpServer.listen(FTP_SRV_PORT);

	String NAME = FTP_SRV_NAME;
	String PASS	= FTP_SRV_PASS;

	/* проверка на наличие измененной конфигурации для FTP-сервера */
	if(NAME.length() == 0) {
		NAME = ap_wifi_ssid;
		PASS = ap_wifi_pwd;
	}

	ftpServer.addUser(NAME, PASS);

	if(UART_MSG) {
		String tmp = "FTP server started on port ";
			   tmp.concat(FTP_SRV_PORT);
			   tmp.concat(".\n");
		Serial.print(tmp);
	}
}

void onStatus(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("status.html");

	auto &vars = tmpl->variables();
	response.sendTemplate(tmpl);
}
void onConfig(HttpRequest &request, HttpResponse &response)
{
	if (request.getRequestMethod() == RequestMethod::POST)
	{
		Settings.max_temp = (byte)request.getPostParameter("max_temp").toInt();
		set_max_temp = Settings.max_temp;

		Settings.ap_mode = request.getPostParameter("ap_mode") == "1";
		Settings.ap_ssid = request.getPostParameter("ap_ssid");
		Settings.ap_psw = request.getPostParameter("ap_psw");

		Settings.st_ssid = request.getPostParameter("st_ssid");
		Settings.st_psw = request.getPostParameter("st_psw");

		Settings.save();

		wifiReInit();
	}

	TemplateFileStream *tmpl = new TemplateFileStream("config.html");
	auto &vars = tmpl->variables();

	vars["max_temp"] = set_max_temp;

	vars["true"] = ap_wifi ? "checked='checked'" : "";
	vars["false"] = !ap_wifi ? "checked='checked'" : "";
	vars["ap_ssid"] = ap_wifi_ssid;
	vars["ap_psw"] = ap_wifi_pwd;

	vars["st_ssid"] = st_wifi_ssid;
	vars["st_psw"] = st_wifi_pwd;

	response.sendTemplate(tmpl);
}
void onAbout(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("about.html");

	auto &vars = tmpl->variables();

	String ap_mac = WifiAccessPoint.getMAC();

	vars["dev_sn"] = convertSN(ap_mac);
	vars["dev_fw"] = FIRMWARE;
	vars["uptime"] = convertTime(uptime, false);
	vars["ap_mac"] = convertMAC(ap_mac);
	vars["st_mac"] = convertMAC(WifiStation.getMAC());

	response.sendTemplate(tmpl);
}
void onFile(HttpRequest &request, HttpResponse &response)
{
	String file = request.getPath();

	if(file[0] == '/')
		file = file.substring(1);

	if(file[0] == '.')
		response.forbidden();
	else
	{
		response.setCache(86400, true);
		response.sendFile(file);
	}
}
void onAjaxStatus(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	json["sens_err"] = (bool)sensor_error;
	json["heater"] = (bool)heater_status;
	json["heater_on"] = convertTime(heater_on, false);
	json["temp"] = (float)temp;
	json["max_temp"] = (byte)max_temp;
	json["ap_mode"] = (bool)ap_wifi;
	json["ap_ssid"] = ap_wifi_ssid;
	json["ap_ip"] = WifiAccessPoint.getIP().toString();
	json["st_ssid"] = WifiStation.getSSID();
	json["st_ip"] = WifiStation.getIP().toString();

	response.sendJsonObject(stream);
}

void startWEBServer()
{
	webServer.listen(WEB_SRV_PORT);
	webServer.addPath("/", onStatus);
	webServer.addPath("/config", onConfig);
	webServer.addPath("/about", onAbout);
	webServer.addPath("/ajax/status", onAjaxStatus);
	webServer.setDefaultHandler(onFile);

	if(UART_MSG) {
		String tmp = "WEB server started on port ";
			   tmp.concat(WEB_SRV_PORT);
			   tmp.concat(".\n");
		Serial.print(tmp);
	}
}

void startServers()
{
	/* метод для запуска серверов */
	startTCPServer();
	startFTPServer();
	startWEBServer();
}

void settingsLoad()
{
	Settings.load();

	if(Settings.exist()) {
		set_max_temp = Settings.max_temp;

		ap_wifi		 = Settings.ap_mode;
		ap_wifi_ssid = Settings.ap_ssid;
		ap_wifi_pwd	 = Settings.ap_psw;

		st_wifi_ssid = Settings.st_ssid;
		st_wifi_pwd	 = Settings.st_psw;

		heater_on	 = Settings.heater_on;
	}
}

void init()
{
	spiffs_mount();
	// монтирование файловой системы

	pinMode(LCP_PIN, OUTPUT);
	// инициализация пина на выход

	load(Off);
	// отключение нагрузки

	System.setCpuFrequency(eCF_160MHz);
	// изменение рабочей частоты процессора на 160 МГц

	settingsLoad();
	// загрузка и применение конфигурации системы

	uartInit();
	// инициализация UART

	wifiInit();
	// инициализация Wi-Fi

	oneWire.begin();
	// инициализация шины 1-Wire

	searchSensor();
	// запуск процедуры поиска датчика

	executeTimer.initializeMs(PERIOD, execute).start();
	// запуск таймера-исполнителя

	autoSaver.initializeMs(60000, autoSave).start();
	// запуск таймера (автоматическое сохранение данных раз в 1 минуту)

	System.onReady(startServers);
	// запуск серверов при полной готовности системы
}
