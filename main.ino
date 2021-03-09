#include <ArduinoJson.h>
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192, 168, 25, 45};
EthernetServer server(80);

void setup()
{
  // Initialize serial port
  Serial.begin(9600);
  while (!Serial)
    continue;

  // Initialize Ethernet libary
  Ethernet.begin(mac, ip);

  // Start to listen
  server.begin();

  Serial.println(F("Server is ready."));
  Serial.print(F("Please connect to http://"));
  Serial.println(Ethernet.localIP());
}

void loop()
{
  // Wait for an incomming connection
  EthernetClient client = server.available();

  // Do we have a client?
  if (!client)
    return;

  Serial.println(F("New client"));

  // Read the request (we ignore the content in this example)
  while (client.available())
    client.read();

  // Allocate a temporary JsonDocument
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<500> doc;

  doc["test"] = 1234;

  Serial.print(F("Sending: "));
  serializeJson(doc, Serial);
  Serial.println();

  // Write response headers
  client.println(F("HTTP/1.0 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connection: close"));
  client.print(F("Content-Length: "));
  client.println(measureJsonPretty(doc));
  client.println();

  // Write JSON document
  serializeJsonPretty(doc, client);

  // Disconnect
  client.stop();
}