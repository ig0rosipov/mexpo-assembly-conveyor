#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>

#include "GyverButton.h"
#include "GyverHacks.h"

#define MANUAL_PIN 2
#define EMERGENCY_PIN 3
#define BUZZER_PIN 4
#define SENSOR_PIN 5
#define CONVEYOR_PIN 6
#define GREEN_LED 7
#define YELLOW_LED 8
#define RED_LED 9

GButton emgBtn(EMERGENCY_PIN);
GButton sensor(SENSOR_PIN);
GButton manualBtn(MANUAL_PIN);

// введите ниже MAC - адрес и IP - адрес вашего контроллера;
// IP-адрес будет зависеть от вашей локальной сети:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192, 168, 64, 254};

EthernetServer server(80);
// задаем переменные для клиента:
char linebuf[80];
int charcount = 0;
byte domain[] = {192, 168, 64, 3};
// byte domain[] = {127, 0, 0, 1};
void setup()
{
  emgBtn.setDebounce(50);
  sensor.setDebounce(50);
  manualBtn.setDebounce(50);

  pinMode(CONVEYOR_PIN, OUTPUT);
  setPin(CONVEYOR_PIN, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(EMERGENCY_PIN, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(MANUAL_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

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

void beep()
{
  setPin(BUZZER_PIN, HIGH);
  setPin(YELLOW_LED, HIGH);
  delay(500);
  setPin(BUZZER_PIN, LOW);
  setPin(YELLOW_LED, LOW);
  delay(500);
}

void warning()
{
  beep();
  beep();
  beep();
}

void loop()
{
  boolean isStopped = false;
  StaticJsonDocument<500> doc;

  Serial.print("EMERGENCY: ");
  Serial.print(readPin(EMERGENCY_PIN));
  Serial.print(" SENSOR: ");
  Serial.print(readPin(SENSOR_PIN));
  Serial.print(" MANUAL: ");
  Serial.println(readPin(MANUAL_PIN));
  delay(100);
  if (readPin(EMERGENCY_PIN) != HIGH && readPin(SENSOR_PIN) != HIGH && readPin(MANUAL_PIN) != LOW)
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
                warning();
                doc["status"] = "running";
                setPin(CONVEYOR_PIN, LOW);
                setPin(GREEN_LED, LOW);
                setPin(YELLOW_LED, LOW);
                setPin(RED_LED, HIGH);
              }
              else if (strstr(linebuf, "GET /stop") > 0)
              {
                warning();
                doc["status"] = "production";
                setPin(CONVEYOR_PIN, HIGH);
                setPin(YELLOW_LED, LOW);
                setPin(RED_LED, LOW);
                setPin(GREEN_LED, HIGH);
              }
              else if (strstr(linebuf, "GET /pause") > 0)
              {
                doc["status"] = "pause";
                setPin(CONVEYOR_PIN, HIGH);
                setPin(GREEN_LED, LOW);
                setPin(RED_LED, LOW);
                setPin(YELLOW_LED, HIGH);
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
      Serial.print(("Sending: "));
      serializeJson(doc, Serial);
      Serial.println();
      // Write response headers
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: application/json");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Access-Control-Allow-Headers: X-Requested-With, content-type, access-control-allow-origin, access-control-allow-methods, access-control-allow-headers");
      client.println("Connection: close");
      client.print("Content-Length: ");
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
      if (readPin(EMERGENCY_PIN) != HIGH && readPin(SENSOR_PIN) != HIGH)
      {
        isStopped = false;
        return;
      }
      return;
    }
    Serial.println(F("Connecting..."));
    EthernetClient client;
    client.setTimeout(10000);
    delay(10);
    if (!client.connect(domain, 80))
    {
      Serial.println(F("Connection failed"));
      client.stop();
      return;
    }
    Serial.println(F("Connected!"));
    if (readPin(SENSOR_PIN) == HIGH)
    {
      client.println(F("GET /api/sensor HTTP/1.0"));
      client.println(F("Host: 192.168.64.3"));
      client.println(F("Connection: close"));
    }
    if (readPin(EMERGENCY_PIN) == HIGH)
    {
      client.println(F("GET /api/emergency HTTP/1.0"));
      client.println(F("Host: 192.168.64.3"));
      client.println(F("Connection: close"));
    }
    if (readPin(MANUAL_PIN) == LOW)
    {
      client.println(F("GET /api/manual HTTP/1.0"));
      client.println(F("Host: 192.168.64.3"));
      client.println(F("Connection: close"));
    }

    if (client.println() == 0)
    {
      Serial.println(F("Failed to send request"));
      client.stop();
      return;
    }

    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    Serial.println(status);
    if (strcmp(status, "HTTP/1.1 200 OK") != 0)
    {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      client.stop();
      delay(1000);
      return;
    }

    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders))
    {
      Serial.println(F("Invalid response"));
      client.stop();
      return;
    }
    isStopped = true;
    client.stop();
    return;
  }
}
