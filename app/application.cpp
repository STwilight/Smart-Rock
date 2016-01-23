#include <user_config.h>
#include <settings.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h>

/* ����������� ����������� �������� */
#define On			 true
#define Off			 false

/* ����������� ����� ��� ����������� ��������� */
#define LCP_PIN		 0		// ��������    = GPIO0
#define TMP_PIN		 2		// 1-Wire ���� = GPIO2

/* ��������� ��� �������� ����� ������� Wi-Fi */
#define AP_WIFI_SSID "Smart Rock"
#define AP_WIFI_PWD	 "12345678"
#define AP_WIFI		 Off

/* ��������� ��� ����������� � Wi-Fi ���� */
/* ���� ���� �� ���������, �� ������ ������� �� ���������� IDE */
#define ST_WIFI_SSID ""
#define ST_WIFI_PWD	 ""
#define ST_WIFI		 On

/* ��������� FTP-������� */
/* ���� ���� ������ � ������ �� ���������, �� ������ (����� � ������)
 * ����� ��������� � �������� ��� ������ ����� ������� */
#define FTP_SRV_NAME ""
#define FTP_SRV_PASS ""
#define FTP_SRV_PORT 21

/* ��������� ����� ��� WEB-������� */
#define WEB_SRV_PORT 80

/* ��������� ����� ��� TCP-������� */
#define TCP_SRV_PORT 9000

/* ��������� ������ ���������� � ������� */
#define UART_ENABLE	 Off
#define UART_MSG	 Off
#define UART_DEBUG	 Off
#define UART_SPEED	 115200

/* �������� �������� */
OneWire oneWire(TMP_PIN);
Timer procTimer;
HttpServer webServer;
FTPServer ftpServer;

/* ���������� ���������� ���������� */
byte addr[8];				// ����� ������� �����������
byte type_s;				// ��� ������� �����������
byte sensor_error = false;	// ���� ������ (������)
byte max_temp = 0;			// ������������ �����������
byte set_max_temp = 0;		// ���������� ������������ �����������
float temp = 0.0;			// ���������� ��� �������� �����������

void load(bool state)
{
	/* ����� ���������� ��������� */
	digitalWrite(LCP_PIN, state);
}
void searchSensor()
{
	/* ����� ������ ������� �� ���� 1-Wire */
	/* ������� ������� ������ ������ ������� */
	byte scan_attempt = 0;

	/* ����� ��� �������� � ������, ���� ROM ������� ������ ������� */
	rescan:

	/* �������� ������� ������ ����� 1-Wire */
	oneWire.reset_search();

	/* ����� ��������� �� ���� 1-Wire */
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

	/* �������� ������������ ������ ������ ������� */
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
			/* ����������� ���� ������� � ��� ����� */
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

			/* ����� ����������� ������ ������� */
			if(UART_ENABLE) {
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
}
float getTemp()
{
	/* ����� ��������� ����������� � ������� */
	float temp = 0;
	byte data[12];

	if(!sensor_error)
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

	if((temp < max_temp) & !sensor_error)
		load(On);
	else
		load(Off);
}

void uartInit()
{
	/* ����� ��������� UART ������ */
	Serial.systemDebugOutput(UART_DEBUG);

	if(UART_ENABLE) {
		Serial.begin(UART_SPEED);
	}
}
void wifiInit()
{
	/* ����� ��������� ������ Wi-Fi ������ */
	String SSID = ST_WIFI_SSID;
	String PWD	= ST_WIFI_PWD;

	/* �������� �� ������� ���������� ������������ ��� ������ ������� */
	if((SSID.length() == 0) or (PWD.length() == 0)) {
		SSID = WIFI_SSID;
		PWD	 = WIFI_PWD;
	}

	/* ���������  */
	WifiStation.enable(ST_WIFI);
	if(ST_WIFI) {
		WifiStation.config(SSID, PWD);
		if(UART_MSG) {
			String tmp = "Client mode enabled. AP SSID: \"";
				   tmp.concat(SSID);
				   tmp.concat("\".\n");
			Serial.print(tmp);
		}
	}

	WifiAccessPoint.enable(AP_WIFI);
	if(AP_WIFI) {
		WifiAccessPoint.config(AP_WIFI_SSID, AP_WIFI_PWD, AUTH_WPA2_PSK);
		WifiAccessPoint.setIP(IPAddress(10, 0, 0, 1));
		if(UART_MSG) {
			String tmp = "AP mode enabled. SSID: \"";
				   tmp.concat(AP_WIFI_SSID);
				   tmp.concat("\".\n");
			Serial.print(tmp);
		}
	}
}

void clientConnected (TcpClient* client)
{
	/* ����� ���������� � �������������� ������� */
	if(UART_MSG) {
		String tmp = "Client connected: ";
			   tmp.concat(client->getRemoteIp().toString().c_str());
			   tmp.concat(".\n");
		Serial.print(tmp);
	}
}
bool clientReceive (TcpClient& client, char *data, int size)
{
	/* ����� ��������� ���������� ������ */
	String r_data = data;
	String tmp;

	/* ���������� software watchdog timer ����� ������ ��������� */
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
		bool loadStatus = digitalRead(LCP_PIN);

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

	/* ����� software watchdog timer */
	//system_soft_wdt_restart();
	WDT.alive();

	return true;
}
void clientDisconnected(TcpClient& client, bool succesfull)
{
	/* ����� ���������� �� ������������� ������� */
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

	/* �������� �� ������� ���������� ������������ ��� FTP-������� */
	if(NAME.length() == 0) {
		NAME = AP_WIFI_SSID;
		PASS = AP_WIFI_PWD;
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
	}

	TemplateFileStream *tmpl = new TemplateFileStream("config.html");
	auto &vars = tmpl->variables();

	vars["max_temp"] = max_temp;

	vars["true"] = AP_WIFI ? "checked='checked'" : "";
	vars["false"] = !AP_WIFI ? "checked='checked'" : "";
	vars["ap_ssid"] = AP_WIFI_SSID;
	vars["ap_psw"] = AP_WIFI_PWD;

	vars["st_ssid"] = ST_WIFI_SSID;
	vars["st_psw"] = ST_WIFI_PWD;

	response.sendTemplate(tmpl);
}
void onAbout(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("about.html");

	auto &vars = tmpl->variables();
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
	json["heater"] = (bool)digitalRead(LCP_PIN);
	json["temp"] = (float)temp;
	json["max_temp"] = (byte)max_temp;
	json["ap_mode"] = (bool)AP_WIFI;
	json["ap_ssid"] = AP_WIFI_SSID;
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
	/* ����� ��� ������� �������� */
	startTCPServer();
	startFTPServer();
	startWEBServer();
}

void init()
{
	spiffs_mount();
	// ������������ �������� �������

	pinMode(LCP_PIN, OUTPUT);
	// ������������� ���� �� �����

	load(Off);
	// ���������� ��������

	uartInit();
	// ������������� UART

	Settings.load();
	// �������� �������� �� �����

	wifiInit();
	// ������������� Wi-Fi

	oneWire.begin();
	// ������������� ���� 1-Wire

	searchSensor();
	// ������ ��������� ������ �������

	procTimer.initializeMs(2000, execute).start();
	// ������ ������� (�������� ����)

	System.onReady(startServers);
	// ������ �������� ��� ������ ���������� �������
}
