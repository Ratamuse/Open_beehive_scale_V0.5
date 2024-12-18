/**
 * @file      ModemMqttPulishExample.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#include <Arduino.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
//#include "utilities.h"

XPowersPMU PMU;



// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#include "utilities.h"
#define MAX_CONNECT_ATTEMPTS 10

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

const char *register_info[] = {
  "Not registered, MT is not currently searching an operator to register to.The GPRS service is disabled, the UE is allowed to attach for GPRS if requested by the user.",
  "Registered, home network.",
  "Not registered, but MT is currently trying to attach or searching an operator to register to. The GPRS service is enabled, but an allowable PLMN is currently not available. The UE will start a GPRS attach as soon as an allowable PLMN is available.",
  "Registration denied, The GPRS service is disabled, the UE is not allowed to attach for GPRS if it is requested by the user.",
  "Unknown.",
  "Registered, roaming.",
};

enum {
  MODEM_CATM = 1,
  MODEM_NB_IOT,
  MODEM_CATM_NBIOT,
};

int bat;


//Mosquitto server address and port
const char server1[] = "ratamuse.hopto.org";
const int port = 1885;
char buffer[1024] = { 0 };


//  4. Copy the <MQTT USERNAME> <MQTT PASSWORD> <CLIENT ID> field to the bottom for replacement
char username[] = "<>";
char password[] = "<>";
char clientID[] = "<>";


int data_channel = 0;

/*HX711*/
#include "HX711.h"

HX711 scale1;
HX711 scale2;
uint8_t dataPin1 = 12;
uint8_t clockPin1 = 11;
uint8_t dataPin2 = 14;
uint8_t clockPin2 = 13;
//float factor1;
//float factor2;

int ledPin = 46;
const int gpioPin = 9;
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "Preferences.h"

const char* ssid = "balance";
const char* password1 = "123456789";

AsyncWebServer server(80);
Preferences preferences;

float lat2      = 0;
float lon2      = 0;
float speed2    = 0;
float alt2      = 0;
int   vsat2     = 0;
int   usat2     = 0;
float accuracy2 = 0;
int   year2     = 0;
int   month2    = 0;
int   day2      = 0;
int   hour2     = 0;
int   min2      = 0;
int   sec2      = 0;
bool  level     = false;

bool isConnect() {
  modem.sendAT("+SMSTATE?");
  if (modem.waitResponse("+SMSTATE: ")) {
    String res = modem.stream.readStringUntil('\r');
    return res.toInt();
  }
  return false;
}

int connectionFailures = 0;

bool setup1Executed = false;


#include "DS18B20.h"


#define ONE_WIRE_BUS              21

OneWire oneWire(ONE_WIRE_BUS);
DS18B20 sensor(&oneWire);



void sendCommonHTML(AsyncWebServerRequest *request, String content) {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background-color: #FFF176; /* Couleur jaune miel */ }";
  html += "h1 { color: #8D6E63; /* Marron foncé */ }"; // Couleur marron foncé pour les titres
  html += "input[type=submit] { background-color: #FFD54F; /* Jaune miel clair */ color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; margin: 10px; }";
  html += "input[type=checkbox] { margin-bottom: 20px; }"; // Espacement vertical pour les cases à cocher
  html += "</style></head><body>";
  html += "<h1>Calibration des balances</h1>";
  html += "<p>Choisir la balance à calibrer</p>";
  html += "<p>Enlever tous poids sur les balances</p>";
  html += content;
  html += "</body></html>";
  request->send(200, "text/html; charset=UTF-8", html);
}

void handleRoot(AsyncWebServerRequest *request) {
  String content = "<div style=\"display: inline-block;\">"; // Div pour aligner les boutons horizontalement
  content += "<form action=\"/calibrate1\" method=\"get\">";
  content += "<input type=\"submit\" value=\"Calibration balance 1\">";
  content += "</form>";
  content += "<form action=\"/calibrate2\" method=\"get\">";
  content += "<input type=\"submit\" value=\"Calibration balance 2\">";
  content += "</form>";
    // Ajout du bouton "Redémarrer l'ESP32"
  content += "<form action=\"/reboot\" method=\"post\">";
  content += "<button type=\"submit\">Redémarrer l'ESP32</button>";
  content += "</form>";
  content += "</div>";
  sendCommonHTML(request, content);
}

void handleCalibration1(AsyncWebServerRequest *request) {
  scale1.tare(20);  // average 20 measurements.
  uint32_t offset1 = scale1.get_offset();
  String content = "<h1>Calibration balance 1</h1>";
  content += "<p>Offset: " + String(offset1) + "</p>";
  content += "<p>Placer un poids sur la balance (en gramme)</p>";
  content += "<form action=\"/weight1\" method=\"get\">";
  content += "<input type=\"text\" name=\"weight\">";
  content += "<input type=\"submit\" value=\"Enter\">";
  content += "</form>";
  sendCommonHTML(request, content);
}

void handleCalibration2(AsyncWebServerRequest *request) {
  scale2.tare(20);  // average 20 measurements.
  uint32_t offset2 = scale2.get_offset();
  String content = "<h1>Calibration balance 2</h1>";
  content += "<p>Offset: " + String(offset2) + "</p>";
  content += "<p>Placer un poids sur la balance (en gramme)</p>";
  content += "<form action=\"/weight2\" method=\"get\">";
  content += "<input type=\"text\" name=\"weight\">";
  content += "<input type=\"submit\" value=\"Enter\">";
  content += "</form>";
  sendCommonHTML(request, content);
}


void handleWeight1(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    scale1.calibrate_scale(weight, 20);
    float scalea = scale1.get_scale();
    uint32_t offset1 = scale1.get_offset();
    message = "<p>Calibration terminée. Factor 1: " + String(scalea, 6) + ", Offset 1: " + String(offset1) + "</p>";

    // Button to trigger ESP32 reboot
    message += "<form action=\"/reboot\" method=\"post\">";
    message += "<button type=\"submit\">Redémarrer l'ESP32</button>";
    message += "</form>";
    
    delay(1000);

    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("factor1", scalea);
    preferences.putFloat("offset1",offset1);
    preferences.end();
  } else {
    message = "<p>Manque paramètres de poids</p>";
  }
  // Add a button to return to the home page
  message += "<form action=\"/\" method=\"get\">";
  message += "<input type=\"submit\" value=\"Retour à l'accueil\">";
  message += "</form>";
  sendCommonHTML(request, message);
}

void handleWeight2(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    scale2.calibrate_scale(weight, 20);
    float scaleb = scale2.get_scale();
    uint32_t offset2 = scale2.get_offset();
    message = "<p>Calibration terminée. Factor 2: " + String(scaleb, 6) + ", Offset 2: " + String(offset2) + "</p>";
    
    // Button to trigger ESP32 reboot
    message += "<form action=\"/reboot\" method=\"post\">";
    message += "<button type=\"submit\">Redémarrer l'ESP32</button>";
    message += "</form>";
    
    delay(1000);
    
    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("factor2", scaleb);
    preferences.putFloat("offset2", offset2);
    preferences.end();
  } else {
    message = "<p>Manque paramètres de poids</p>";
  }
  // Add a button to return to the home page
  message += "<form action=\"/\" method=\"get\">";
  message += "<input type=\"submit\" value=\"Retour à l'accueil\">";
  message += "</form>";
  sendCommonHTML(request, message);
}

void handleReboot(AsyncWebServerRequest *request) {
  
    esp_restart(); // Directly trigger reboot for regular form submissions
  
}

void initPMU() {

    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
    Serial.println("Failed to initialize power.....");
    while (1) {
      delay(4000);
    }
  }
  //bat = PMU.getBatteryPercent();
  //Set the working voltage of the modem, please do not modify the parameters
  PMU.setDC5Voltage(3300);  //SIM7080 Modem main power channel 2700~ 3400V
  PMU.enableDC5();

}

void setup1(){
      Serial.begin(115200);
       //Start while waiting for Serial monitoring
  while (!Serial);
        // Si la broche est basse (LOW), exécutez la fonction "calibration"
        initPMU();
pinMode(ledPin, OUTPUT);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
        scale1.begin(dataPin1, clockPin1);
        scale2.begin(dataPin2, clockPin2);

  // Créer un point d'accès WiFi au lieu de se connecter à un réseau WiFi
  WiFi.softAP(ssid, password1);
  Serial.println("Point d'accès WiFi créé");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/calibrate1", HTTP_GET, handleCalibration1);
  server.on("/calibrate2", HTTP_GET, handleCalibration2);
  server.on("/weight1", HTTP_GET, handleWeight1);
  server.on("/weight2", HTTP_GET, handleWeight2);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.begin();

  setup1Executed = true;
  
        
    }

void setup() {

  pinMode(gpioPin, INPUT_PULLUP);

    // Lecture de l'état de la broche GPIO 12
    

    if (digitalRead(gpioPin) == LOW) {
setup1();
}
else{
                 

  Serial.begin(115200);

  //Start while waiting for Serial monitoring
  while (!Serial);

  // delay(3000);

  Serial.println();

  /*********************************
     *  step 1 : Initialize power chip,
     *  turn on modem and gps antenna power channel
    ***********************************/
  if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
    Serial.println("Failed to initialize power.....");
    while (1) {
      delay(4000);
    }
  }
  //Set the working voltage of the modem, please do not modify the parameters
  PMU.setDC3Voltage(3000);  //SIM7080 Modem main power channel 2700~ 3400V
  PMU.enableDC3();
  PMU.setDC5Voltage(3300);  //SIM7080 Modem main power channel 2700~ 3400V
  PMU.enableDC5();
pinMode(ledPin, OUTPUT);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_11,1);
/*
  //Modem GPS Power channel
  PMU.setBLDO2Voltage(3300);
  PMU.enableBLDO2();  //The antenna power must be turned on to use the GPS function
*/
  PMU.enableBattVoltageMeasure();
  PMU.enableSystemVoltageMeasure();
  // TS Pin detection must be disable, otherwise it cannot be charged
  PMU.disableTSPinMeasure();

  /*********************************
     * step 2 : start modem
    ***********************************/

  Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

  pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
  pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);
  pinMode(BOARD_MODEM_RI_PIN, INPUT);

  int retry = 0;
  while (!modem.testAT(1000)) {
    Serial.print(".");
    if (retry++ > 6) {
      // Pull down PWRKEY for more than 1 second according to manual requirements
      digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
      delay(100);
      digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
      delay(1000);
      digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
      retry = 0;
      Serial.println("Retry start modem .");
    }
  }
  Serial.println();
  Serial.print("Modem started!");

  /*********************************
     * step 3 : Check if the SIM card is inserted
    ***********************************/
  String result;


  if (modem.getSimStatus() != SIM_READY) {
    Serial.println("SIM Card is not insert!!!");
    return;
  }


  /*********************************
     * step 4 : Set the network mode to NB-IOT
    ***********************************/

  modem.setNetworkMode(2);  //use automatic

  modem.setPreferredMode(MODEM_NB_IOT);

  uint8_t pre = modem.getPreferredMode();

  uint8_t mode = modem.getNetworkMode();

  Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);


 /*********************************
    * step 5 : Wait for the network registration to succeed
    ***********************************/
  RegStatus s;
  int attempts = 0; // Ajoutez un compteur pour les tentatives de connexion
  do {
    s = modem.getRegistrationStatus();
    if (s != REG_OK_HOME && s != REG_OK_ROAMING) {
      Serial.print(".");
      delay(1000);
      attempts++; // Incrémentez le compteur à chaque tentative
      if (attempts >= 10) { // Si 10 tentatives ont échoué
        esp_sleep_enable_timer_wakeup(30 * 60 * 1000000LL); // Mettez l'ESP32-S3 en deep sleep pendant 30 minutes
        esp_deep_sleep_start();
      }
    }

  } while (s != REG_OK_HOME && s != REG_OK_ROAMING);

  Serial.println();
  Serial.print("Network register info:");
  Serial.println(register_info[s]);

  

  bool res = modem.isGprsConnected();
  if (!res) {
    modem.sendAT("+CNACT=0,1");
    if (modem.waitResponse() != 1) {
      Serial.println("Activate network bearer Failed!");
      return;
    }
    
  }
  modem.sendAT("+CGNSPWR=<0>");
  Serial.print("GPRS status:");
  Serial.println(res ? "connected" : "not connected");

  String ccid = modem.getSimCCID();
  Serial.print("CCID:");
  Serial.println(ccid);

  String imei = modem.getIMEI();
  Serial.print("IMEI:");
  Serial.println(imei);

  String imsi = modem.getIMSI();
  Serial.print("IMSI:");
  Serial.println(imsi);

  String cop = modem.getOperator();
  Serial.print("Operator:");
  Serial.println(cop);

  IPAddress local = modem.localIP();
  Serial.print("Local IP:");
  Serial.println(local);

  int csq = modem.getSignalQuality();
  Serial.print("Signal quality:");
  Serial.println(csq);


char buffer[1024] = {0};

// Envoyer la commande AT pour obtenir l'heure
modem.sendAT("+CCLK?");
if (modem.waitResponse(1000L, "+CCLK: ") != 1) {
    // Gestion de l'erreur
    Serial.println("Erreur lors de la récupération de l'heure");

    } else {
        // Supprimer le préfixe "+CCLK: " et le suffixe de fin de ligne de la réponse
        char *timeStr = buffer + 7;
        timeStr[strlen(timeStr) - 2] = '\0';
        Serial.println(timeStr);
    
 // Extraire l'heure de timeStr
            int hour = atoi(timeStr + 9);

            // Si l'heure est comprise entre 08:00 et 09:00, exécutez la fonction getGPSData()
            if (hour >= 8 && hour < 9) {
//Modem GPS Power channel
  PMU.setBLDO2Voltage(3300);
  PMU.enableBLDO2();  //The antenna power must be turned on to use the GPS function

            
            /*********************************
     * step 3 : start modem gps function
    ***********************************/

    //  When configuring GNSS, you need to stop GPS first
    modem.disableGPS();
    delay(500);

#if 0
    /*
    ! GNSS Work Mode Set
     <gps mode> GPS work mode.
        1 Start GPS NMEA out.
    <glo mode> GLONASS work mode.
        0 Stop GLONASS NMEA out.
        1 Start GLONASS NMEA out.
    <bd mode> BEIDOU work mode.
        0 Stop BEIDOU NMEA out.
        1 Start BEIDOU NMEA out.
    <gal mode> GALILEAN work mode.
        0 Stop GALILEAN NMEA out.
        1 Start GALILEAN NMEA out.
    <qzss mode> QZSS work mode.
        0 Stop QZSS NMEA out.
        1 Start QZSS NMEA out.*/

    //GNSS Work Mode Set GPS+BEIDOU
    modem.sendAT("+CGNSMOD=1,0,0,1,0");
    modem.waitResponse();


    /*
    GNSS Command,For more parameters, see <SIM7070_SIM7080_SIM7090 Series_AT Command Manual> 212 page.
    <minInterval> range: 1000-60000 ms
     minInterval is the minimum time interval in milliseconds that must elapse between position reports. default value is 1000.
    <minDistance> range: 0-1000
     Minimum distance in meters that must be traversed between position reports. Setting this interval to 0 will be a pure time-based tracking/batching.
    <accuracy>:
        0  Accuracy is not specified, use default.
        1  Low Accuracy for location is acceptable.
        2 Medium Accuracy for location is acceptable.
        3 Only High Accuracy for location is acceptable.
    */
    // minInterval = 1000,minDistance = 0,accuracy = 0
    modem.sendAT("+SGNSCMD=2,1000,0,3");
    modem.waitResponse();

    // Turn off GNSS.
    modem.sendAT("+SGNSCMD=0");
    modem.waitResponse();
#endif
    delay(500);

    // GPS function needs to be enabled for the first use
    if (modem.enableGPS() == false) {
        Serial.print("Modem enable gps function failed!!");
        while (1) {
            delay(5000);
        }
    }
            }
    }


  /*********************************
    * step 6 : setup MQTT Client
    ***********************************/

  // If it is already connected, disconnect it first
  modem.sendAT("+SMDISC");
  modem.waitResponse();


  snprintf(buffer, 1024, "+SMCONF=\"URL\",\"%s\",%d", server1, port);
  modem.sendAT(buffer);
  if (modem.waitResponse() != 1) {
    return;
  }
  snprintf(buffer, 1024, "+SMCONF=\"USERNAME\",\"%s\"", username);
  modem.sendAT(buffer);
  if (modem.waitResponse() != 1) {
    return;
  }

  snprintf(buffer, 1024, "+SMCONF=\"PASSWORD\",\"%s\"", password);
  modem.sendAT(buffer);
  if (modem.waitResponse() != 1) {
    return;
  }

  snprintf(buffer, 1024, "+SMCONF=\"CLIENTID\",\"%s\"", clientID);
  modem.sendAT(buffer);
  if (modem.waitResponse() != 1) {
    return;
  }

  int8_t ret;
  int connectAttempts = 0;

  do {
    modem.sendAT("+SMCONN");
    ret = modem.waitResponse(10000);
    if (ret != 1) {
      Serial.println("La connexion a échoué, nouvelle tentative de connexion...");
      delay(1000);
      connectAttempts++;
    }

    // Si le nombre maximum de tentatives de connexion est atteint, Entrer en mode de veille profonde pendant 30 minutes
    if (connectAttempts >= MAX_CONNECT_ATTEMPTS) {
      Serial.println("La connexion a échoué après plusieurs tentatives, redémarrage de l'ESP32...");
      modem.sendAT("+CPOWD=<1>");
      Serial.println("Modem stoppé");
      //delay(1000);
       PMU.disableDC3();
       PMU.disableDC5();
  
  esp_deep_sleep_start();
    }

  } while (ret != 1);

  Serial.println("Le client MQTT est connecté!");

  
/*********************************
    * step 6 : setup scales
    ***********************************/



// Initialiser les préférences
  preferences.begin("my-app", false);
    
  // Récupérer la valeur de "factor1"
  float factor1 = preferences.getFloat("factor1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
  
  // Récupérer la valeur de "factor2"
  float factor2 = preferences.getFloat("factor2", 0.0); // 0.0 est la valeur par défaut si "factor2" n'est pas trouvé

 // Récupérer la valeur de "offset1"
float offset1 = preferences.getFloat("offset1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé

// Récupérer la valeur de "offset1"
float offset2 = preferences.getFloat("offset2", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
    
  // Terminer l'utilisation des préférences
  preferences.end();
    
  // Utiliser factor1 et factor2 dans votre code
  Serial.println("Factor 1: " + String(factor1, 6));
  Serial.println("Factor 2: " + String(factor2, 6));
 // Utiliser offset1 et offset2 dans votre code
  Serial.println("Offset 1: " + String(offset1, 6));
  Serial.println("Offset 2: " + String(offset2, 6));

  scale1.begin(dataPin1, clockPin1);
  scale1.set_scale(factor1);


  scale2.begin(dataPin2, clockPin2);
  scale2.set_scale(factor2);
  
}
sensor.begin();

}

void loop() {

if (setup1Executed) {
    while (true) {
      // Empêcher l'exécution du code de la boucle loop
    }

}
  if (!isConnect()) {
    Serial.println("MQTT Client disconnect!");
    delay(100);
  }
  Serial.print("getBatteryPercent:");
  Serial.print(PMU.getBatteryPercent());
  Serial.println("%");
  sensor.requestTemperatures();

  //  wait until sensor is ready
  while (!sensor.isConversionComplete())
  {
    delay(1);
  }

  Serial.print("Temp: ");
  Serial.println(sensor.getTempC());


char buffer[1024] = {0};

// Envoyer la commande AT pour obtenir l'heure
modem.sendAT("+CCLK?");
if (modem.waitResponse(1000L, "+CCLK: ") != 1) {
    // Gestion de l'erreur
    Serial.println("Erreur lors de la récupération de l'heure");

    } else {
        // Supprimer le préfixe "+CCLK: " et le suffixe de fin de ligne de la réponse
        char *timeStr = buffer + 7;
        timeStr[strlen(timeStr) - 2] = '\0';
        Serial.println(timeStr);
    
 // Extraire l'heure de timeStr
            int hour = atoi(timeStr + 9);

            // Si l'heure est comprise entre 08:00 et 09:00, exécutez la fonction getGPSData()
            if (hour >= 8 && hour < 9) {
    if (modem.getGPS(&lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                     &year2, &month2, &day2, &hour2, &min2, &sec2)) {
        Serial.println();
        Serial.print("lat:"); Serial.print(String(lat2, 8)); Serial.print("\t");
        Serial.print("lon:"); Serial.print(String(lon2, 8)); Serial.println();
        Serial.print("speed:"); Serial.print(speed2); Serial.print("\t");
        Serial.print("altitude:"); Serial.print(alt2); Serial.println();
        Serial.print("year:"); Serial.print(year2);
        Serial.print(" montoh:"); Serial.print(month2);
        Serial.print(" day:"); Serial.print(day2);
        Serial.print(" hour:"); Serial.print(hour2);
        Serial.print(" minutes:"); Serial.print(min2);
        Serial.print(" second:"); Serial.print(sec2); Serial.println();
        Serial.println();
            }
            
            }
    }



// Initialiser les préférences
  preferences.begin("my-app", false);
    
  // Récupérer la valeur de "factor1"
  float factor1 = preferences.getFloat("factor1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
  
  // Récupérer la valeur de "factor2"
  float factor2 = preferences.getFloat("factor2", 0.0); // 0.0 est la valeur par défaut si "factor2" n'est pas trouvé

 // Récupérer la valeur de "offset1"
float offset1 = preferences.getFloat("offset1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé

// Récupérer la valeur de "offset1"
float offset2 = preferences.getFloat("offset2", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
    
  // Terminer l'utilisation des préférences
  preferences.end();

int scaleA = 0;
  if (scale1.is_ready())
    delay(200);
  {
    scaleA = static_cast<int>(((scale1.read_average(10)-offset1)/factor1));
    
   Serial.print("ScaleA:");
    Serial.println(scaleA);
  }

  delay(200);


int scaleB = 0;

  if (scale2.is_ready());
    delay(200);
  {
   scaleB = static_cast<int>(((scale2.read_average(10)-offset2)/factor2));
    Serial.print("ScaleB:");
    Serial.println(scaleB);

    //Serial.println(scale2.get_units(1));
  }

  delay(200);

int temp1 = 0;
  Serial.println();
  temp1 = sensor.getTempC();
  String payload1 = "";
  payload1.concat(temp1);
  payload1.concat("\r\n");
  
  
  String payload2 = "";
  payload2.concat(scaleA);
  payload2.concat("\r\n");

String payload3 = "";
  payload3.concat(scaleB);
  payload3.concat("\r\n");


  bat = PMU.getBatteryPercent();
  String payload4 = "";
  payload4.concat(bat);
  payload4.concat("\r\n");


  // AT+SMPUB=<topic>,<content length>,<qos>,<retain><CR>message is enteredQuit edit mode if messagelength equals to <contentlength>
  
  snprintf(buffer, 1024, "+SMPUB=\"G1/%s/temp1/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload1.length());
  modem.sendAT(buffer);
  if (modem.waitResponse(">") == 1) {
    modem.stream.write(payload1.c_str(), payload1.length());
    Serial.print("Try publish payload1: ");
    Serial.println(payload1);

    if (modem.waitResponse(3000)) {
      Serial.println("Send Packet1 success!");
    } else {
      Serial.println("Send Packet1 failed!");
    }
  }
  delay(100);
  
  
  snprintf(buffer, 1024, "+SMPUB=\"G1/%s/scaleA/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload2.length());
  modem.sendAT(buffer);
  if (modem.waitResponse(">") == 1) {
    modem.stream.write(payload2.c_str(), payload2.length());
    Serial.print("Try publish payload2: ");
    Serial.println(payload2);

    if (modem.waitResponse(3000)) {
      Serial.println("Send Packet2 success!");
    } else {
      Serial.println("Send Packet2 failed!");
    }
  }
delay(100);


  
  snprintf(buffer, 1024, "+SMPUB=\"G1/%s/scaleB/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload3.length());
  modem.sendAT(buffer);
  if (modem.waitResponse(">") == 1) {
    modem.stream.write(payload3.c_str(), payload3.length());
    Serial.print("Try publish payload3: ");
    Serial.println(payload3);

    if (modem.waitResponse(3000)) {
      Serial.println("Send Packet3 success!");
    } else {
      Serial.println("Send Packet3 failed!");
    }
  }
delay(100);



  snprintf(buffer, 1024, "+SMPUB=\"G1/%s/bat/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload4.length());
  modem.sendAT(buffer);
  if (modem.waitResponse(">") == 1) {
    modem.stream.write(payload4.c_str(), payload4.length());
    Serial.print("Try publish payload4: ");
    Serial.println(payload4);

    if (modem.waitResponse(3000)) {
      Serial.println("Send Packet4 success!");
    } else {
      Serial.println("Send Packet4 failed!");
    }
  }
  modem.sendAT("+SMDISC");

  // Entrer en mode de veille profonde pendant 30 secondes
  Serial.println("Entering deep sleep for 30 minutes...");
  esp_sleep_enable_timer_wakeup(1800 * 1000000);  // 30 secondes en microsecondes 1800s
  modem.sendAT("+CPOWD=<1>");
  Serial.println("Modem stoppé");
  delay(100);
  digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
delay(50);
digitalWrite(ledPin, HIGH);
delay(50);
digitalWrite(ledPin, LOW);
delay(50);
  //PMU.disableBLDO2();
  PMU.disableDC3();
  PMU.disableDC5();
  
  esp_deep_sleep_start();
}





