#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>

#define CONVEYOR_PIN 7
#define SENSOR_PIN 6

// введите ниже MAC-адрес и IP-адрес вашего контроллера;
// IP-адрес будет зависеть от вашей локальной сети:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// byte ip[] = {192, 168, 25, 45};
EthernetServer server(80);

// задаем переменные для клиента:
char linebuf[80];
int charcount = 0;

void setup()
{

  pinMode(CONVEYOR_PIN, OUTPUT);
  digitalWrite(CONVEYOR_PIN, LOW);

  // Initialize serial port
  Serial.begin(9600);
  while (!Serial)
    continue;

  // Initialize Ethernet libary
  Ethernet.begin(mac);

  // Start to listen
  server.begin();

  Serial.println(F("Server is ready."));
  Serial.print(F("Please connect to http://"));
  Serial.println(Ethernet.localIP());
}

// Показываем веб-страницу с кнопкой «вкл/выкл» для реле:

void loop()
{
  StaticJsonDocument<500> doc;
  // прослушиваем входящих клиентов:
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

          if (strstr(linebuf, "GET /run") > 0)
          {
            digitalWrite(CONVEYOR_PIN, HIGH);
            doc["status"] = "running";
          }
          else if (strstr(linebuf, "GET /stop") > 0)
          {
            digitalWrite(CONVEYOR_PIN, LOW);
            doc["status"] = "production";
          }
          else if (strstr(linebuf, "GET /check") > 0)
          {
            if (digitalRead(CONVEYOR_PIN) == HIGH)
            {
              doc["status"] = "running";
            }
            else
            {
              doc["status"] = "production";
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
    Serial.println("client disonnected"); //  "Клиент отключен"
  }
}