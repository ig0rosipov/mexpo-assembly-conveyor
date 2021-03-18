#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>
// #include "GyverTimer.h"
#include "GyverButton.h"
#include "GyverHacks.h"

// GTimer productionTimer(MS);
// GTimer runningTimer(MS);

#define CONVEYOR_PIN 7
#define INDICATOR_PIN 6
#define EMERGENCY_PIN 3
#define IDK_PIN 4

GButton emgBtn(EMERGENCY_PIN);
GButton idkBtn(IDK_PIN);
GButton thirdBtn(3);

// введите ниже MAC - адрес и IP - адрес вашего контроллера;
// IP-адрес будет зависеть от вашей локальной сети:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// byte ip[] = {192, 168, 25, 45};
EthernetServer server(80);
// задаем переменные для клиента:
char linebuf[80];
int charcount = 0;
char domain[] = "bel11.modern.org";
boolean isStopped;

void setup()
{
  emgBtn.setDebounce(100);
  idkBtn.setDebounce(100);
  thirdBtn.setDebounce(100);

  Serial.begin(9600);
  pinMode(7, OUTPUT);
  pinMode(6, OUTPUT);
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
  Ethernet.begin(mac);
  server.begin();

  Serial.println(F("Server is ready."));
  Serial.print(F("Please connect to http://"));
  Serial.println(Ethernet.localIP());

  // Serial.println(F("Connecting..."));

  // EthernetClient client;
  // client.setTimeout(10000);
  // if (!client.connect("bel11.modern.org:5000", 80))
  // {
  //   Serial.println(F("Connection failed"));
  //   return;
  // }

  // Serial.println(F("Connected!"));
}

void handleEmergencyButton()
{
  digitalWrite(CONVEYOR_PIN, LOW);
  while (digitalRead(EMERGENCY_PIN) == LOW)
  {
    setPin(INDICATOR_PIN, HIGH);
    delay(250);
    setPin(INDICATOR_PIN, LOW);
    delay(250);
    if (digitalRead(EMERGENCY_PIN) == HIGH)
    {
      break;
    }
  }
}

void loop()
{
  StaticJsonDocument<500> doc;

  while (readPin(EMERGENCY_PIN) == HIGH)
  {
    isStopped = false;
    EthernetClient client = server.available();
    if (client)
    {
      if (readPin(EMERGENCY_PIN) == LOW)
      {
        handleEmergencyButton();
      }
      else
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
                if (readPin(EMERGENCY_PIN) == LOW)
                {
                  handleEmergencyButton();
                }
                else
                {
                  if (strstr(linebuf, "GET /run") > 0)
                  {
                    setPin(CONVEYOR_PIN, HIGH);
                    doc["status"] = "running";
                  }
                  else if (strstr(linebuf, "GET /stop") > 0)
                  {
                    setPin(CONVEYOR_PIN, LOW);
                    doc["status"] = "production";
                  }
                  else if (strstr(linebuf, "GET /check") > 0)
                  {
                    if (readPin(CONVEYOR_PIN) == HIGH)
                    {
                      doc["status"] = "running";
                    }
                    else
                    {
                      doc["status"] = "production";
                    }
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
      Serial.println("client disonnected"); //  "Клиент отключен"
    }
  }
  Serial.println("Emergency!" + isStopped);
  if (!isStopped)
  {
    Serial.println(F("Connecting..."));
    delay(2000);
    EthernetClient client;
    client.setTimeout(10000);
    if (!client.connect(domain, 5000))
    {
      Serial.println(F("Connection failed"));
      return;
    }
    Serial.println(F("Connected!"));

    client.println(F("GET /emergency HTTP/1.0"));
    client.println(F("Host: bel11.modern.org"));
    client.println(F("Connection: close"));
    isStopped = true;
    if (client.println() == 0)
    {
      Serial.println(F("Failed to send request"));
      client.stop();
      return;
    }

    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0)
    {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      client.stop();
      return;
    }

    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders))
    {
      Serial.println(F("Invalid response"));
      client.stop();
      return;
    }

    client.stop();
  }
  Serial.print("Conveyor stopped by emergency: ");
  Serial.println(isStopped);
}