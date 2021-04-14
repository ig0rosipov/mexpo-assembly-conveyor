#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>

#include "GyverButton.h"
#include "GyverHacks.h"

#define CONVEYOR_PIN 7
#define SENSOR_PIN 6
#define EMERGENCY_PIN 3
#define IDK_PIN 4

GButton emgBtn(EMERGENCY_PIN);
GButton sensor(SENSOR_PIN);

// введите ниже MAC - адрес и IP - адрес вашего контроллера;
// IP-адрес будет зависеть от вашей локальной сети:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192, 168, 24, 186};
EthernetServer server(80);
// задаем переменные для клиента:
char linebuf[80];
int charcount = 0;
byte domain[] = {192, 168, 25, 111};

void setup()
{
  emgBtn.setDebounce(80);
  sensor.setDebounce(80);

  Serial.begin(9600);
  pinMode(7, OUTPUT);
  pinMode(6, INPUT);
  pinMode(5, INPUT);
  pinMode(4, INPUT);
  pinMode(3, INPUT);
  pinMode(2, INPUT);
  pinMode(1, INPUT);
  pinMode(0, INPUT);

  Serial.begin(9600);
  while (!Serial)
    continue;

  // Initialize Ethernet libary
  Ethernet.begin(mac, ip);
  server.begin();

  Serial.println(F("Server is ready."));
  Serial.print(F("Please connect to http://"));
  Serial.println(Ethernet.localIP());
}

void loop()
{
  boolean isStopped = false;
  StaticJsonDocument<500> doc;

  if (readPin(EMERGENCY_PIN) == HIGH && readPin(SENSOR_PIN) == HIGH)
  {
    isStopped = false;
    EthernetClient client = server.available();
    if (client)
    {
      Serial.println("new client"); //  "новый клиент"
      memset(linebuf, 0, sizeof(linebuf));
      charcount = 0;
      // HTTP-запрос заканчивается пустой строкой:
      boolean currentLineIsBlank = true;
      while (client.connected())
      {
        if (client.available())
        {
          char c = client.read();
          // считываем HTTP-запрос, символ за символом:
          linebuf[charcount] = c;
          if (charcount < sizeof(linebuf) - 1)
            charcount++;
          // если вы дошли до конца строки (т.е. если получили
          // символ новой строки), это значит,
          // что HTTP-запрос завершен, и вы можете отправить ответ:
          if (c == '\n' && currentLineIsBlank)
          {
            break;
          }
          if (c == '\n')
          {
            if (strstr(linebuf, "GET /") > 0)
            {
              if (strstr(linebuf, "GET /run") > 0)
              {
                setPin(CONVEYOR_PIN, LOW);
                doc["status"] = "running";
              }
              else if (strstr(linebuf, "GET /stop") > 0)
              {
                setPin(CONVEYOR_PIN, HIGH);
                doc["status"] = "production";
              }
              else if (strstr(linebuf, "GET /check") > 0)
              {
                if (readPin(CONVEYOR_PIN) == LOW)
                {
                  doc["status"] = "running";
                }
                else
                {
                  doc["status"] = "production";
                }
              }
            }

            // если получили символ новой строки...
            currentLineIsBlank = true;
            memset(linebuf, 0, sizeof(linebuf));
            charcount = 0;
          }
          else if (c != '\r')
          {
            // если получили какой-то другой символ...
            currentLineIsBlank = false;
          }
        }
      }
      // даем веб-браузеру время на получение данных:
      delay(1);
      // закрываем соединение:
      Serial.print(F("Sending: "));
      serializeJson(doc, Serial);
      Serial.println();
      // Write response headers
      client.println(F("HTTP/1.0 200 OK"));
      client.println(F("Content-Type: application/json"));
      client.println(F("Access-Control-Allow-Origin: *"));
      client.println(F("Access-Control-Allow-Headers: X-Requested-With, content-type, access-control-allow-origin, access-control-allow-methods, access-control-allow-headers"));
      client.println(F("Connection: close"));
      client.print(F("Content-Length: "));
      client.println(measureJsonPretty(doc));
      client.println();
      // Write JSON document
      serializeJsonPretty(doc, client);
      client.stop();
    }
  }
  else
  {
    setPin(CONVEYOR_PIN, HIGH);
    // Serial.println("Emergency!");
    // Serial.print("isStopped: ");
    // Serial.println(isStopped);

    if (isStopped == true)
    {
      if (readPin(EMERGENCY_PIN) == HIGH && readPin(SENSOR_PIN) == HIGH)
      {
        isStopped = false;
        return;
      }
      return;
    }
    Serial.println(F("Connecting..."));
    delay(10);
    EthernetClient client;
    // client.setTimeout(10000);
    if (!client.connect(domain, 80))
    {
      Serial.println(F("Connection failed"));
      client.stop();
      return;
    }
    // Serial.println(F("Connected!"));
    // Serial.print("Sensor Pin: ");
    // Serial.println(readPin(SENSOR_PIN));
    // Serial.print("Emegrency Pin: ");
    // Serial.println(readPin(EMERGENCY_PIN));
    if (readPin(SENSOR_PIN) == LOW)
    {
      client.println(F("GET /api/sensor HTTP/1.0"));
      client.println(F("Host: 192.168.24.83"));
      client.println(F("Connection: close"));
    }
    if (readPin(EMERGENCY_PIN) == LOW)
    {
      client.println(F("GET /api/emergency HTTP/1.0"));
      client.println(F("Host: 192.168.24.83"));
      client.println(F("Connection: close"));
    }

    if (client.println() == 0)
    {
      Serial.println(F("Failed to send request"));
      client.stop();
      return;
    }

    // char status[32] = {0};
    // client.readBytesUntil('\r', status, sizeof(status));
    // Serial.println(status);
    // if (strcmp(status, "HTTP/1.1 200 OK") != 0)
    // {
    //   Serial.print(F("Unexpected response: "));
    //   Serial.println(status);
    //   client.stop();
    //   delay(1000);
    //   return;
    // }

    // char endOfHeaders[] = "\r\n\r\n";
    // if (!client.find(endOfHeaders))
    // {
    //   Serial.println(F("Invalid response"));
    //   client.stop();
    //   return;
    // }
    isStopped = true;
    client.stop();
    return;
  }
}