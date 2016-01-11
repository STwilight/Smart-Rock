#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/OneWire/OneWire.h>


/* ����������� ����������� �������� */
#define On			 true
#define Off			 false

/* ����������� ����� ��� ����������� ��������� */
#define LCP_PIN		 0		// ��������    = GPIO0
#define TMP_PIN		 2		// 1-Wire ���� = GPIO2

/* ������ ��� ����������� � Wi-Fi ���� */
#define AP_WIFI_SSID "Smart Rock"
#define AP_WIFI_PWD	 "12345678"

/* ��������� ����� ��� TCP ������� */
#define TCP_SRV_PORT 9000

/* �������� �������� */
OneWire oneWire(TMP_PIN);
Timer procTimer;


// *** WEB_SERVER_CODE_START
HttpServer server;
FTPServer ftp;

int inputs[] = {0, 2}; // Set input GPIO pins here
Vector<String> namesInput;
const int countInputs = sizeof(inputs) /  sizeof(inputs[0]);
// *** WEB_SERVER_CODE_END


/* ���������� ���������� ���������� */
byte addr[8];			// ����� ������� �����������
byte type_s;			// ��� ������� �����������
byte error = false;		// ���� ������ (������)
byte max_temp = 30;		// ������������ �����������
byte set_max_temp = 0;	// ���������� ������������ �����������
float temp = 0.0;		// ���������� ��� �������� �����������
bool UART = false;		// ���������� ������ ���������� � �������

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
		Serial.print("No sensors found!\n");
		oneWire.reset_search();
		delay(500);
		/* ����� software watchdog timer */
		WDT.alive();
	}

	/* �������� ������������ ������ ������ ������� */
	if(OneWire::crc8(addr, 7) != addr[7])
	{
		if(UART)
			Serial.print("Sensor's ROM CRC is not valid!\n");
		goto rescan;
	}
	else
	{
		/* ����������� ���� ������� � ��� ����� */
		if(UART) {
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
	//WifiStation.enable(false);


	// *** WEB_SERVER_CODE_START
	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD);
	// *** WEB_SERVER_CODE_END


	WifiAccessPoint.enable(true);
	WifiAccessPoint.config(AP_WIFI_SSID, AP_WIFI_PWD, AUTH_WPA2_PSK);
	WifiAccessPoint.setIP(IPAddress(10, 0, 0, 1));
	if(UART) {
		String tmp = "AP mode enabled. SSID: \"";
			   tmp.concat(AP_WIFI_SSID);
			   tmp.concat("\".\n");
		Serial.print(tmp);
	}
}

void clientConnected (TcpClient* client)
{
	/* ����� ���������� � �������������� ������� */
	if(UART) {
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
	system_soft_wdt_stop();

	r_data.toLowerCase();
	if(r_data.indexOf("\n") != -1)
		r_data.replace("\n", "");
	else if(r_data.indexOf("\r") != -1)
		r_data.replace("\r", "");

	if(UART) {
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

		if(UART) {
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

		if(UART) {
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
	system_soft_wdt_restart();

	return true;
}
void clientDisconnected(TcpClient& client, bool succesfull)
{
	/* ����� ���������� �� ������������� ������� */
	if(UART) {
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
	if(UART) {
		String tmp = "TCP server started. Address is ";
			   tmp.concat("10.0.0.1:");
			   tmp.concat(TCP_SRV_PORT);
			   tmp.concat(".\n");
		Serial.print(tmp);
	}
}


// *** WEB_SERVER_CODE_START
void onIndex(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("index.html");
	auto &vars = tmpl->variables();
	//vars["counter"] = String(counter);
	response.sendTemplate(tmpl); // this template object will be deleted automatically
}

void onFile(HttpRequest &request, HttpResponse &response)
{
	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);

	if (file[0] == '.')
		response.forbidden();
	else
	{
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

void onAjaxInput(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	json["status"] = (bool)true;

	String stringKey = "StringKey";
	String stringValue = "StringValue";

	json[stringKey] = stringValue;

    for( int i = 0; i < 11; i++)
    {
        char buff[3];
        itoa(i, buff, 10);
        String desiredString = "sensor_";
        desiredString += buff;
        json[desiredString] = desiredString;
    }


	JsonObject& gpio = json.createNestedObject("gpio");
	for (int i = 0; i < countInputs; i++)
		gpio[namesInput[i].c_str()] = digitalRead(inputs[i]);

	response.sendJsonObject(stream);
}

void onAjaxFrequency(HttpRequest &request, HttpResponse &response)
{
	int freq = request.getQueryParameter("value").toInt();
	System.setCpuFrequency((CpuFrequency)freq);

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	json["status"] = (bool)true;
	json["value"] = (int)System.getCpuFrequency();

	response.sendJsonObject(stream);
}

void startWebServer()
{
	server.listen(80);
	server.addPath("/", onIndex);
	server.addPath("/ajax/input", onAjaxInput);
	server.addPath("/ajax/frequency", onAjaxFrequency);
	server.setDefaultHandler(onFile);

	Serial.println("\r\n=== WEB SERVER STARTED ===");
	Serial.println(WifiStation.getIP());
	Serial.println("==============================\r\n");
}

void startFTP()
{
	if (!fileExist("index.html"))
		fileSetContent("index.html", "<h3>Please connect to FTP and upload files from folder 'web/build' (details in code)</h3>");

	// Start FTP server
	ftp.listen(21);
	ftp.addUser("me", "123"); // FTP account
}

// Will be called when WiFi station was connected to AP
void connectOk()
{
	Serial.println("I'm CONNECTED");

	startFTP();
	startWebServer();
}
// *** WEB_SERVER_CODE_END


void init()
{
	spiffs_mount();
	// ������������ �������� �������

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


	// *** WEB_SERVER_CODE_START
	for (int i = 0; i < countInputs; i++)
	{
		namesInput.add(String(inputs[i]));
		pinMode(inputs[i], INPUT);
	}
	// Run our method when station was connected to AP
	WifiStation.waitConnection(connectOk);
	// *** WEB_SERVER_CODE_END


	Serial.println("System initialization completed successfully.");
}
