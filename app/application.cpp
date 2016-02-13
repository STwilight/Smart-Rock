
/* подключение заголовочных файлов Sming */
#include <user_config.h>
#include <settings.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h>

/* подключение внешних заголовочных файлов C */
extern "C" {
	#include "user_interface.h"
	uint16 readvdd33(void);
}

/* версия программного обеспечения модуля */
#define FIRMWARE	 "1.0 Beta"

/* определение специальных значений */
#define On			 true
#define Off			 false

/* определение выводов для подключения периферии */
#define LCP_PIN		 0		// нагрузка    = GPIO0
#define TMP_PIN		 2		// 1-Wire шина = GPIO2

/* определение параметров питания */
float vcc_minimal  = 3.0;	// минимально допустимое напряжение питания (в волтах)
int vcc_correction = -100;  // погрешность измерения напряжения питания (в миливольтах)

/* переменные для отсчета времени */
uint32_t uptime	   = 0;		// время работы системы (в секундах)
uint32_t heater_on = 0;		// время работы обогревателя (в секундах)

/* настройки по-умолчанию для режима точки доступа Wi-Fi */
String ap_wifi_ssid		= "Smart Rock";
String ap_wifi_pwd		= "12345678";
bool   ap_wifi			= Off;
String ap_wifi_def_ssid	= "Smart Rock [Unconfigured]";
bool   ap_wifi_def  	= false;

/* настройки по-умолчанию для режима клиента Wi-Fi сети */
/* если поля не заполнены, то данные берутся из переменных IDE (WIFI_SSID, WIFI_PWD)*/
String st_wifi_ssid = "";
String st_wifi_pwd  = "";
bool   st_wifi		= On;
bool   st_wifi_err  = false;

/* настройки WEB-сервера */
#define WEB_SRV_PORT 80

/* настройки FTP-сервера */
/* если поля логина и пароля не заполнены, то данные (логин и пароль)
 * будут совпадать с данныеми для режима точки доступа */
#define FTP_SRV_NAME "Smart Rock"
#define FTP_SRV_PASS "12345678"
#define FTP_SRV_PORT 21
#define FTP_SRV		 On

/* настройки TCP-сервера */
#define TCP_SRV_PORT 9000
#define TCP_SRV		 Off

/* настройка вывода информации в консоль */
#define UART_ENABLE	 Off
#define UART_MSG	 Off
#define UART_DEBUG	 Off
#define UART_SPEED	 115200

/* объявление и создание объектов */
OneWire oneWire(TMP_PIN);
Timer executeTimer, autoSaver;
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
String ap_mac, st_mac;		// переменные для хранения MAC-адресов Wi-Fi модуля
String module_sn;			// переменная для хранения серийного номера;

void load(bool state)
{
	/* метод управления состоянием нагрузки (обогреватель) */
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
			/* определение типа датчика и его вывод в UART */
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

			/* вывод уникального адреса датчика в UART */
			if(UART_ENABLE) {
				Serial.print("Sensor's ROM is:");
				for(byte i = 0; i < 8; i++)
				{
					Serial.write(' ');
					Serial.print(addr[i], HEX);
				}
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
		/* отправка команды запуска преобразования (паразитное питание) */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0x44, 1);

		/* пауза для выполнения датчиком измерений */
		WDT.alive();
		delay(450);

		/* отправка команды чтения температуры с датчика */
		oneWire.reset();
		oneWire.select(addr);
		oneWire.write(0xBE);

		/* чтение данных о температуре */
		for (byte i = 0; i < 9; i++)
			data[i] = oneWire.read();

		/* конвертация температуры */
		int16_t raw = (data[1] << 8) | data[0];
		if(type_s)
		{
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
	/* метод вывода темпаратуры в UART */
	String tmp = "Temperature is ";
		   tmp.concat(temperature);
		   tmp.concat("C.");
	Serial.print(tmp);
}

void execute()
{
	/* метод-исполнитель, выполняющийся по таймеру */
	/* получение температуры нагрузки (обогреватель) */
	temp = getTemp();

	/* обновление максимальной температуры */
	if(max_temp != set_max_temp)
		max_temp = set_max_temp;

	/* изменение состояния нагрузки (обогреватель) */
	if((temp < max_temp) & !sensor_error)
		load(On);
	else
		load(Off);

	/* обновление значения счетчика времени работы нагрузки (обогреватель) */
	heater_status = digitalRead(LCP_PIN);
	if(heater_status)
		heater_on++;

	/* обновление значения счетчика времени работы системы */
	uptime++;
}
void autoSave()
{
	/* метод для автоматического сохранения данных о времени работы нагрузки (обогреватель) */
	Settings.heater_on = heater_on;
	Settings.save();
}

String convertTime(uint32_t time)
{
	/* функция для конвертации времени из uint_32 значения в секундах в String дни, часы:минуты:секунды */
	String days;
	String hours;
	String minutes;
	String seconds;

	String result;
	char buffer[20];

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

	return result;
}
String convertMAC(String macAddress)
{
	/* функция для конвертации MAC адреса с разделением двоеточием */
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
String convertHEX(String hexSN)
{
	/* метод конвертации HEX строки вида "1a2b3c" в DEC значение */
	const char symbols[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
							  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

	String hexNumber = hexSN;
	byte hexLength = hexNumber.length();
	uint32_t decResult = 0;

	hexNumber.toUpperCase();
	for(int i=0; i<hexLength; i++)
		for(int j=0; j<sizeof(symbols); j++) {
			if(hexNumber.charAt(i) == symbols[j])
				decResult += j << 4*(hexLength-i-1);
	}

	return String(decResult);
}
String convertSN(String macAddress)
{
	/* метод конвертации 3 последних байт MAC-адреса в десятичный серийный номер */
	String tmp;

	char macChar[macAddress.length() + 1];
	macAddress.toCharArray(macChar, macAddress.length() + 1);

	byte size = sizeof(macChar) - 1;
	for(int i=6; i<size; i++)
		tmp += macChar[i];

	/* необходимо использовать конвертацию из String в uint32_t для преобразования HEX > DEC
	 * посредством использования недоступной сейчас в Sming функции sscanf()
	 *
	 * временное решение – самописный конвертер HEX-кода в DEC String значение
	 * функция для вызова: (String) convertHEX(String hexSN)
	 *
	 * правильное решение:
	 * 		sscanf(tmp.c_str(), "%x", &result);
	 * 		return (long) result; */

	return convertHEX(tmp);
}

void uartInit()
{
	/* метод настройки UART модуля */
	Serial.systemDebugOutput(UART_DEBUG);

	if(UART_ENABLE) {
		Serial.begin(UART_SPEED);
	}
}

void wifiConnectOK()
{
	/* метод, выполняющийся при успешном подключении к Wi-Fi точке доступа */
	st_wifi_err = false;

	if(WifiAccessPoint.isEnabled() & !ap_wifi) {
		WifiAccessPoint.enable(Off);
		ap_wifi_def = false;
	}

	if(UART_MSG) {
		String tmp = "Successful connection to \"";
			   tmp.concat(WifiStation.getSSID());
			   tmp.concat("\". Obtained IP: ");
			   tmp.concat(WifiStation.getIP().toString());
		Serial.print(tmp);
	}
}
void wifiConnectFail()
{
	/* метод, выполняющийся при отсутствии подключения к Wi-Fi точке доступа */
	st_wifi_err = true;

	if(WifiStation.isEnabled())
		WifiStation.enable(Off);

	if(!WifiAccessPoint.isEnabled()) {
		WifiAccessPoint.enable(On);
		WifiAccessPoint.config(ap_wifi_def_ssid, "", AUTH_OPEN);
		ap_wifi_def = true;
	}

	if(UART_MSG)
		Serial.print("Failed to connect to Wi-Fi network!");
}
void wifiInit()
{
	/* метод настройки режима Wi-Fi модуля */
	/* проверка наличия изменений в конфигурации для режима клиента Wi-Fi сети от значений по-умолчанию */
	if((st_wifi_ssid.length() == 0) or (st_wifi_pwd.length() == 0)) {
		st_wifi_ssid = WIFI_SSID;
		st_wifi_pwd	 = WIFI_PWD;
	}

	/* настройка режима клиента */
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

	/* настройка режима точки доступа */
	WifiAccessPoint.enable(ap_wifi);
	if(ap_wifi) {
		WifiAccessPoint.setIP(IPAddress(10, 0, 0, 1));
		WifiAccessPoint.config(ap_wifi_ssid, ap_wifi_pwd, AUTH_WPA2_PSK);
		if(UART_MSG) {
			String tmp = "AP mode enabled. SSID: \"";
				   tmp.concat(ap_wifi_ssid);
				   tmp.concat("\".\n");
			Serial.print(tmp);
		}
	}

	/* ожидание подключения к точке доступа Wi-Fi */
	WifiStation.waitConnection(wifiConnectOK, 20, wifiConnectFail);
}
void wifiReInit()
{
	/* метод перенастройки режима Wi-Fi модуля */
	/* получение сохраненных параметров */
	Settings.load();

	/* применение параметров */
	if(Settings.exist()) {
		/* применение параметров для режима клиента */
		if((st_wifi_ssid != Settings.st_ssid) or (st_wifi_pwd != Settings.st_psw)) {
			st_wifi_ssid = Settings.st_ssid;
			st_wifi_pwd = Settings.st_psw;
			WDT.enable(false);
			if(!WifiStation.isEnabled())
				WifiStation.enable(On);
			if(WifiStation.isConnected())
				WifiStation.disconnect();
			WifiStation.config(Settings.st_ssid, Settings.st_psw);
			WDT.alive();
		}

		/* применение параметров для режима точки доступа */
		if((ap_wifi != Settings.ap_mode) or (ap_wifi_ssid != Settings.ap_ssid) or (ap_wifi_pwd != Settings.ap_psw)) {
			if(ap_wifi == Settings.ap_mode) {
				if(ap_wifi) {
					ap_wifi_ssid = Settings.ap_ssid;
					ap_wifi_pwd = Settings.ap_psw;
					WDT.enable(false);
					WifiAccessPoint.config(ap_wifi_ssid, ap_wifi_pwd, AUTH_WPA2_PSK);
					WDT.alive();
				}
			}
			else {
				/* Необходимый IP адрес в режиме AP назначается лишь в случае конфигурации при перезапуске системы.
				 * Временная мера – перезагрузка системы при изменении состояния точки доступа (вкл/выкл). */
				if(executeTimer.isStarted())
					executeTimer.stop();
				if(autoSaver.isStarted())
					autoSaver.stop();
				spiffs_unmount();
				System.restart();
			}
		}

		/* ожидание подключения к точке доступа Wi-Fi */
		WifiStation.waitConnection(wifiConnectOK, 20, wifiConnectFail);
	}
}

void clientConnected (TcpClient* client)
{
	/* вывод информации в UART о подключившемся к TCP-серверу клиенте */
	if(UART_MSG) {
		String tmp = "Client connected: ";
			   tmp.concat(client->getRemoteIp().toString().c_str());
			   tmp.concat(".\n");
		Serial.print(tmp);
	}
}
bool clientReceive (TcpClient& client, char *data, int size)
{
	/* метод обработки полученных TCP-сервером команд */
	String r_data = data;
	String tmp;

	/* отключение software watchdog timer перед длительной операцией */
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
	/* вывод информации в UART об отключившемся от TCP-сервера клиенте */
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
	/* метод для запуска TCP-сервера */
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
	/* метод для запуска FTP-сервера */
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
	/* метод выдачи Status-страницы */
	TemplateFileStream *tmpl = new TemplateFileStream("status.html");

	auto &vars = tmpl->variables();
	response.sendTemplate(tmpl);
}
void onConfig(HttpRequest &request, HttpResponse &response)
{
	/* метод выдачи Config-страницы
	 * отвечает за выдачу/сохранение настроек модуля через WEB-интерфейс */

	/* сохранение переданных в POST-запросе параметров */
	if (request.getRequestMethod() == RequestMethod::POST)
	{
		Settings.max_temp = (byte)request.getPostParameter("max_temp").toInt();
		set_max_temp = Settings.max_temp;

		Settings.ap_mode = request.getPostParameter("ap_mode") == "1";
		Settings.ap_ssid = request.getPostParameter("ap_ssid");
		Settings.ap_psw = request.getPostParameter("ap_psw");

		Settings.st_ssid = request.getPostParameter("st_ssid");
		Settings.st_psw = request.getPostParameter("st_psw");

		autoSaver.stop();
		Settings.save();
		autoSaver.start(true);

		wifiReInit();
	}

	/* выдача текущей конфигурации модуля */
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
void onService(HttpRequest &request, HttpResponse &response)
{
	/* метод выдачи Service-страницы */
	TemplateFileStream *tmpl = new TemplateFileStream("service.html");

	auto &vars = tmpl->variables();
	response.sendTemplate(tmpl);
}
void onAbout(HttpRequest &request, HttpResponse &response)
{
	/* метод выдачи About-страницы */
	TemplateFileStream *tmpl = new TemplateFileStream("about.html");

	auto &vars = tmpl->variables();

	vars["dev_sn"] = module_sn;
	vars["dev_fw"] = FIRMWARE;
	vars["uptime"] = convertTime(uptime);
	vars["ap_mac"] = ap_mac;
	vars["st_mac"] = st_mac;

	response.sendTemplate(tmpl);
}
void onFile(HttpRequest &request, HttpResponse &response)
{
	/* метод выдачи файла */
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
	/* метод генерации и выдачи статуса устройства в формате JSON */
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	json["heater"] = (bool)heater_status;
	json["heater_on"] = convertTime(heater_on);
	json["sens_err"] = (bool)sensor_error;
	json["temp"] = (float)temp;
	json["max_temp"] = (byte)max_temp;
	json["ap_mode"] = (bool)WifiAccessPoint.isEnabled();

	if(ap_wifi_def)
		json["ap_ssid"] = ap_wifi_def_ssid;
	else
		json["ap_ssid"] = ap_wifi_ssid;

	json["ap_ip"] = WifiAccessPoint.getIP().toString();
	json["st_err"] = (bool)st_wifi_err;
	json["st_ssid"] = WifiStation.getSSID();
	json["st_ip"] = WifiStation.getIP().toString();

	response.sendJsonObject(stream);
}
void onAjaxService(HttpRequest &request, HttpResponse &response)
{
	/* метод генерации и выдачи статуса устройства для Service-страницы в формате JSON */
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	uint32_t ADC_Value = 0;

	WDT.enable(false);
	for(byte i=0; i<100; i++)
		ADC_Value += readvdd33();
	WDT.alive();

	ADC_Value /= 100;

	json["adc_val"] = ADC_Value;

	ADC_Value += vcc_correction;

	json["vcc_val"] = (float)(ADC_Value/1000.0);
	json["vcc_min"] = (float)vcc_minimal;
	json["vcc_corr"] = vcc_correction;
	json["heater"] = (bool)heater_status;
	json["heater_on"] = heater_on;
	json["sens_err"] = (bool)sensor_error;
	json["temp"] = (float)temp;
	json["max_temp"] = (byte)max_temp;
	json["uptime"] = convertTime(uptime);

	response.sendJsonObject(stream);
}

void startWEBServer()
{
	/* метод для запуска WEB-сервера */
	webServer.listen(WEB_SRV_PORT);
	webServer.addPath("/", onStatus);
	webServer.addPath("/config", onConfig);
	webServer.addPath("/service", onService);
	webServer.addPath("/about", onAbout);
	webServer.addPath("/ajax/status", onAjaxStatus);
	webServer.addPath("/ajax/service", onAjaxService);
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
	if(TCP_SRV)
		startTCPServer();
	if(FTP_SRV)
		startFTPServer();
	startWEBServer();
}

void settingsLoad()
{
	/* метод для загрузки и применения сохраненных параметров */
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

void valuesInit()
{
	/* метод инициализации значений для выдачи (SN, MAC-адреса) */
	String tmp = WifiAccessPoint.getMAC();

	ap_mac = convertMAC(tmp);
	st_mac = convertMAC(WifiStation.getMAC());
	module_sn = convertSN(tmp);
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

	uartInit();
	// инициализация UART

	settingsLoad();
	// загрузка и применение конфигурации системы

	wifiInit();
	// инициализация Wi-Fi

	valuesInit();
	// инициализация значений для выдачи (SN, MAC-адреса)

	oneWire.begin();
	// инициализация шины 1-Wire

	searchSensor();
	// запуск процедуры поиска датчика

	executeTimer.initializeMs(1000, execute).start();
	// запуск таймера-исполнителя (выполняется раз в секунду)

	autoSaver.initializeMs(60000, autoSave).start();
	// запуск таймера (автоматическое сохранение данных раз в 1 минуту)

	System.onReady(startServers);
	// запуск серверов при полной готовности системы
}
