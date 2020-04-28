/*
    Modification of project posted by hwhardsoft
    https://create.arduino.cc/projecthub/hwhardsoft/covid19-realtime-monitor-5f6920?ref=search&ref_id=tft&offset=35

    Modified to work with ESP8266
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <ArduinoHttpClient.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "arduino_secrets.h"

#define TFT_CS D0  //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
#define TFT_DC D8  //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
#define TFT_RST -1 //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
#define TS_CS D3   //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)

#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK "your-password"
#endif

const char *ssid = STASSID;
const char *password = STAPSK;

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30 * 1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 2000;

const char *host = "www.worldometers.info";
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = "CC:F9:E0:C1:D0:3A:8D:89:39:42:46:54:48:70:F4:6F:55:C8:92:A2";

/*____Calibrate Touchscreen_____*/
#define MINPRESSURE 10 // minimum required force for touch event
#define TS_MINX 370
#define TS_MINY 470
#define TS_MAXX 3700
#define TS_MAXY 3600
/*______End of Calibration______*/

int infected = 0;
int recovered = 0;
int deaths = 0;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

WiFiClientSecure client;
HttpClient http(client, "www.worldometers.info", 443);

void setup()
{
  Serial.begin(115200);
  // initialize the TFT
  Serial.println("Init TFT ...");
  tft.begin();
  tft.setRotation(3);            // landscape mode
  tft.fillScreen(ILI9341_BLACK); // clear screen

  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("connecting to ");
  tft.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    tft.print(".");
  }
  tft.println("");
  tft.println("WiFi connected");
  tft.print("IP address: ");
  tft.println(WiFi.localIP());

  client.setFingerprint(fingerprint);
}

void loop()
{
  check_country("Canada");
  delay(2000);
  check_country("US");
  delay(2000);
  check_country("Mexico");
  delay(2000);
}

void draw_country_screen(String sCountry)
{
  tft.fillScreen(ILI9341_BLACK); // clear screen

  // headline
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.print(sCountry + ":");

  // infected
  tft.setCursor(10, 70);
  tft.setTextColor(ILI9341_RED);
  tft.print("Infected:");
  tft.setCursor(200, 70);
  tft.print(infected);

  // recovered
  tft.setCursor(10, 130);
  tft.setTextColor(ILI9341_GREEN);
  tft.print("Recovered:");
  tft.setCursor(200, 130);
  tft.print(recovered);

  // deaths
  tft.setCursor(10, 190);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.print("Deaths:");
  tft.setCursor(200, 190);
  tft.print(deaths);
}

void check_country(String sCountry)
{
  int err = 0;
  int readcounter = 0;
  int read_value_step = 0;
  String s1 = "";
  String s2 = "";

  err = http.get("/coronavirus/country/" + sCountry + "/");
  if (err == 0)
  {
    Serial.println("startedRequest ok");

    err = http.responseStatusCode();
    if (err >= 0)
    {
      Serial.print("Got status code: ");
      Serial.println(err);

      // Usually you'd check that the response code is 200 or a
      // similar "success" code (200-299) before carrying on,
      // but we'll print out whatever response we get

      // If you are interesting in the response headers, you
      // can read them here:
      //while(http.headerAvailable())
      //{
      //  String headerName = http.readHeaderName();
      //  String headerValue = http.readHeaderValue();
      //}

      Serial.print("Request data for ");
      Serial.println(sCountry);

      // Now we've got to the body, so we can print it out
      unsigned long timeoutStart = millis();
      char c;
      // Whilst we haven't timed out & haven't reached the end of the body
      while ((http.connected() || http.available()) &&
             (!http.endOfBodyReached()) &&
             ((millis() - timeoutStart) < kNetworkTimeout))
      {
        if (http.available())
        {
          c = http.read();
          s2 = s2 + c;
          if (readcounter < 255)
          {
            readcounter++;
          }
          else
          {
            readcounter = 0;
            String tempString = "";
            tempString.concat(s1);
            tempString.concat(s2);
            // check infected first
            if (read_value_step == 0)
            {
              int place = tempString.indexOf("Coronavirus Cases:");
              if ((place != -1) && (place < 350))
              {
                read_value_step = 1;
                s2 = tempString.substring(place + 15);
                tempString = s2.substring(s2.indexOf("#aaa") + 6);
                s1 = tempString.substring(0, (tempString.indexOf("</")));
                s1.remove(s1.indexOf(","), 1);
                Serial.print("Coronavirus Cases: ");
                Serial.println(s1);
                infected = s1.toInt();
              }
            }
            // check deaths
            if (read_value_step == 1)
            {
              int place = tempString.indexOf("Deaths:");
              if ((place != -1) && (place < 350))
              {
                read_value_step = 2;
                s2 = tempString.substring(place + 15);
                tempString = s2.substring(s2.indexOf("<span>") + 6);
                s1 = tempString.substring(0, (tempString.indexOf("</")));
                s1.remove(s1.indexOf(","), 1);
                Serial.print("Deaths: ");
                Serial.println(s1);
                deaths = s1.toInt();
              }
            }
            // check recovered
            if (read_value_step == 2)
            {
              int place = tempString.indexOf("Recovered:");
              if ((place != -1) && (place < 350))
              {
                s2 = tempString.substring(place + 15);
                tempString = s2.substring(s2.indexOf("<span>") + 6);
                s1 = tempString.substring(0, (tempString.indexOf("</")));
                s1.remove(s1.indexOf(","), 1);
                Serial.print("Recovered: ");
                Serial.println(s1);
                recovered = s1.toInt();
                draw_country_screen(sCountry);
                http.stop();
                return;
              }
            }

            s1 = s2;
            s2 = "";
          }

          // We read something, reset the timeout counter
          timeoutStart = millis();
        }
        else
        {
          // We haven't got any data, so let's pause to allow some to
          // arrive
          delay(kNetworkDelay);
        }
      }
    }
    else
    {
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  }
  else
  {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }
  http.stop();
}