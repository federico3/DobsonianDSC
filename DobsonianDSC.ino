/*
  Very Low Cost Digital Setting Circles
  This is a simple adapter to connect an inexpensive incremental optical encoder
  and an accelerometer+compass (LSM303DLHC) via WiFi to the SkySafari Android/iOS app,
  in order to provide "Push To" style features to dobsonian style telescope mounts
  including tabletops.
  Copyright (c) 2017 Vladimir Atehortua. All rights reserved.
  This program is free software: you can redistribute it and/or modify
  it under the terms of the version 3 GNU General Public License as
  published by the Free Software Foundation.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with this program.
  If not, see <http://www.gnu.org/licenses/>
*/

/**
    Hardware used:
    NodeMCU ESP8266 development board (version 12E) I used the HiLetgo model https://www.amazon.com/gp/product/B010O1G1ES
    ESP8266 pinout:
     Azimuth_Encoder_A = GPIO4 = PIN_D2
     Azimuth_Encoder_B = GPIO5 = PIN_D1
     Elevation_Encoder_A = GPIO12 = PIN_D6
     Elevation_Encoder_B = GPIO14 = PIN_D7
     VIN = Encoder VCC = 5V power source
*/

#include <Encoder.h>  // Install this on Arduino IDE: "Encoder Library by Paul Stoffregen" (I used version 1.4.1), https://www.pjrc.com/teensy/td_libs_Encoder.html
#include <ESP8266WiFi.h>  // built in library from Arduino IDE
#define RADIAN_TO_STEPS   1623.3804f  // ratio to convert from radians to encoder steps, using a "full circle" of 10200 steps as the Skysafari setting for basic encoder
#define STEPS_IN_FULL_CIRCLE  10200    // number of steps in a full circle, should match the skysafari setting for basic encoder

WiFiServer server(4030);    // 4030 is the default port Skysafari uses for WiFi connection to telescopes
WiFiClient remoteClient;    // represents the connection to the remote app (Skysafari)
#define WiFi_Access_Point_Name "FedericoDob"   // Name of the WiFi access point this device will create for your tablet/phone to connect to.

Encoder myEnc_azimuth(4, 5);
Encoder myEnc_elevation(12, 14);

void setup()
{
  Serial.begin(57600);
  Serial.println("\nESP266 boot");

  WiFi.mode(WIFI_AP);
  IPAddress ip(1, 2, 3, 4);     // The "telescope IP address" that Skysafari should connect to is 1.2.3.4 which is easy to remember.
  IPAddress gateway(1, 2, 3, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(ip, gateway, subnet);
  WiFi.softAP(WiFi_Access_Point_Name);

  IPAddress myIP = WiFi.softAPIP();

  // tcp listener to receive incoming connections from Skysafari:
  server.begin();
  server.setNoDelay(true);
}

long before, after;
void loop()
{
#ifdef TIMEPROFILE
  after = millis();
  if (after - before > 0)
  {
    Serial.print("TIME: ");
    Serial.println((after - before));
  }
#endif
  attendTcpRequests();  // gets priority to prevent timeouts on Skysafari. Measured AVG execution time = 18ms
  yield();

#ifdef TIMEPROFILE
  before = millis();
#endif
  yield();  // altough ESP8266 is supposed to do its work background TCP/wiFi after each loop, yieldig here can't hurt
}

void attendTcpRequests()
{
  // check for new or lost connections:
  if (server.hasClient())
  {
    Serial.println("hasClient!");
    if (!remoteClient || !remoteClient.connected())
    {
      if (remoteClient)
      {
        Serial.print("Client Disconnected\n");
        remoteClient.stop();
      }
      remoteClient = server.available();
      //Serial.print("Inbound connection from: ");
      //Serial.println(remoteClient.remoteIP());
      //  remoteClient.flush();
      remoteClient.setNoDelay(true);
    }
  }

  // when we have a new incoming connection from Skysafari:
  while (remoteClient.available())
  {
    byte skySafariCommand = remoteClient.read();

    if (skySafariCommand == 81)  // 81 is ascii for Q, which is the only command skysafari sends to "basic encoders"
    {
      char encoderResponse[20];
      //int iAzimuthReading = imu.smoothAzimuthReading;
      long iAzimuthReading = myEnc_azimuth.read();
      long iAltitudeReading = myEnc_elevation.read();
      // int iAltitudeReading = imu.smoothAltitudeReading;
      sprintf(encoderResponse, "%i\t%i\t\n", iAzimuthReading, iAltitudeReading);
      Serial.println(encoderResponse);

      remoteClient.println(encoderResponse);
    }
    else if (skySafariCommand == 72) // 'H' - request for encoder resolution, e.g. 10000-10000\n
    {
      char response[20];
      // Resolution on both axis is equal
      snprintf(response, 20, "%u-%u", STEPS_IN_FULL_CIRCLE, STEPS_IN_FULL_CIRCLE);
      Serial.println(response);

      remoteClient.println(response);

    }
    else
    {
      Serial.print("*****");
      Serial.println(skySafariCommand);
    }
  }
}
