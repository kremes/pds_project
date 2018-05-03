//#define REDUCED_MODES
#include <WiFi.h>
#include <PubSubClient.h>
#include "WS2812FX.h"
#include <ArduinoJson.h>

#define PIR_PIN 5
#define LEDSTRIP_PIN 4

#define LED_COUNT 144
#define EFFECTS_SIZE 58
#define WIFI_MAX_RECONNECT_TIME 60000 //v millis

// WIFI + MQTT nastavenia
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* mqtt_server = "192.168.69.69";
const char* mqtt_username = "pdsproject";
const char* mqtt_password = "pdsproject";
const char* mqtt_ledstrip_topic = "pdsproject/office/ledstrip";
const char* mqtt_ledstrip_set_topic = "pdsproject/office/ledstrip/set";
const char* mqtt_pir_topic = "pdsproject/office/pir";
const char* mqtt_on = "ON";
const char* mqtt_off = "OFF";
const int mqtt_keepalive = 5000;  //v millis

// zoznam efektov
char* effects[]={"static", "blink", "breath", "color wipe", "color wipe random", "random color", "easter", "dynamic", 
  "rainbow", "rainbow cycle", "scan", "dual scan", "fade", "theater chase", "theater chase rainbow", "running lights", 
  "twinkle", "twinkle random",  "twinkle fade", "twinkle fade random", "sparkle", "flash sparkle", "hyper sparkle", 
  "strobe", "strobe rainbow", "multi strobe", "blink rainbow", "android", "chase color", "chase random", "chase rainbow", 
  "chase flash", "chase flash random", "chase rainbow white", "colorful", "traffic light", "color sweep random", 
  "running color", "running red blue", "running random", "larson scanner", "comet", "fireworks", "fireworks random",
  "merry christmas", "fire flicker", "gradient", "loading", "dual color wipe in out", "dual color wipe in in",
  "dual color wipe out out", "dual color wipe out in", "circus combustus", "custom chase", "cc on rainbow", 
  "cc on rainbow cycle", "cc blink", "cc random"};

// pomocne premenne
WiFiClient espClient;
PubSubClient client(espClient);
WS2812FX ws2812fx = WS2812FX();

long last_msg_time = 0; //kvoli keepalive
bool old_pir_value = true;
bool wifi_reconnect = false;
long wifi_reconnect_start = 0;

uint8_t led_brightness = 30;
uint32_t led_color = 0xffffff;
uint8_t led_effect = 0;
bool led_state = false;
uint8_t led_speed = 150; //KONSTANTNE ZATIAL

bool flash = false;
uint8_t flash_brightness = 255;
uint32_t flash_color = 0xff0000;
uint8_t flash_effect = 23; //23==multi strobe
uint8_t flash_speed = 255; //KONSTANTNE ZATIAL
uint32_t flash_length = 0;
long flash_start_time = 0;

/** 
 * Inicializacia pri starte
 */
void setup() {
  Serial.begin(115200);
  
  Serial.println("WS2812FX setup");
  ws2812fx.init(false, LED_COUNT, LEDSTRIP_PIN);  
  update_led();
  ws2812fx.start();

  Serial.println("Wifi setup:");
  setup_wifi();
  
  pinMode(PIR_PIN, INPUT);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}

/** 
 * Pripojenie na wifi
 */
void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  wifi_reconnect_start = millis();
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED && millis()-wifi_reconnect_start < WIFI_MAX_RECONNECT_TIME) {
    delay(1000);
    Serial.print(".");
  }
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    setup_wifi();
  }
  else {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

/** 
 * Nastav LED pasik podla globalnych premennych
 */
void  update_led() {
  if(!led_state) {  //vypnute svetlo
    ws2812fx.setBrightness(0);
    ws2812fx.setColor(0x000000);
    return;
  }
  
  ws2812fx.setBrightness(led_brightness);
  ws2812fx.setSpeed(led_speed);
  ws2812fx.setColor(led_color);
  ws2812fx.setMode(led_effect);
}

/** 
 * Handle na prijatie MQTT spravy zo subscribe topicov
 * @param topic z ktoreho topicu
 * @param payload sprava
 * @param length dlzka spravy
 */
void callback(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  
  for (int i = 0; i < length; i++) {
    msg[i] = payload[i];
  }
  msg[length] = '\0';
  
  Serial.print("[LED] RECV: ");
  Serial.println(msg);
  
  DynamicJsonBuffer dynamicJsonBuffer;
  JsonObject& root = dynamicJsonBuffer.parseObject(payload);

  if (root.containsKey("flash")) { //ak sa jedna o flash
    flash_length = (uint32_t)root["flash"] * 1000;
    if (root.containsKey("brightness")) {
      flash_brightness = (uint8_t)root["brightness"];
    }
    else {
      flash_brightness = led_brightness;
    }
    
    if (root.containsKey("color")) {
      float bright_fix;
      if(flash_brightness>0)
      {
        bright_fix = 255/(float)flash_brightness;
      }
      else
      {
        bright_fix = 0;
      }
      
      uint8_t red = (uint8_t)root["color"]["r"]*bright_fix;
      uint8_t green = (uint8_t)root["color"]["g"]*bright_fix;
      uint8_t blue = (uint8_t)root["color"]["b"]*bright_fix;
      flash_color = red<<16 | green<<8 | blue;
    }
    else {
      flash_color = led_color;
    }

    if (root.containsKey("effect")) {
      int i;
      for(i=0; i<EFFECTS_SIZE; i++) {
        if (strcmp(root["effect"], effects[i]) == 0) {
          flash_effect = i;
          break;
        }
      }
    } 
    
    flash = true;
  }
  else {
    flash = false;
    if (root.containsKey("state")) {
      if (strcmp(root["state"], mqtt_on) == 0) {
        led_state = 1;
      } else if (strcmp(root["state"], mqtt_off) == 0) {
        led_state = 0;
      }
    }

    if (root.containsKey("brightness")) {
      led_brightness = (uint8_t)root["brightness"];
    }
    
    if (root.containsKey("color")) {
      float bright_fix;
      if(led_brightness>0)
      {
        bright_fix = 255/(float)led_brightness;
      }
      else
      {
        bright_fix = 0;
      }
      
      uint8_t red = (uint8_t)root["color"]["r"]*bright_fix;
      uint8_t green = (uint8_t)root["color"]["g"]*bright_fix;
      uint8_t blue = (uint8_t)root["color"]["b"]*bright_fix;
      led_color = red<<16 | green<<8 | blue;
    }

    if (root.containsKey("effect")) {
      int i;
      for(i=0; i<EFFECTS_SIZE; i++) {
        if (strcmp(root["effect"], effects[i]) == 0) {
          led_effect = i;
          break;
        }
      }
    } 
  }
  
  if (!root.containsKey("flash")){
    update_led();
  }
  led_send_state();
}

/** 
 * Pripojenie na MQTT server
 */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(mqtt_ledstrip_set_topic);
      led_send_state();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/** 
 * Odoslanie spravy o nastaveni LED pasiku na MQTT server
 */
void led_send_state() {
  DynamicJsonBuffer dynamicJsonBuffer;

  JsonObject& root = dynamicJsonBuffer.createObject();

  if(flash) {
    root["state"] = mqtt_on;
    JsonObject& color = root.createNestedObject("color");
    color["r"] = (uint8_t)(flash_color>>16);
    color["g"] = (uint8_t)(flash_color>>8);
    color["b"] = (uint8_t)(flash_color>>0);
  
    root["brightness"] = flash_brightness;
    root["effect"] = effects[flash_effect];
  }
  else {
    root["state"] = (led_state) ? mqtt_on : mqtt_off;
    JsonObject& color = root.createNestedObject("color");
    color["r"] = (uint8_t)(led_color>>16);
    color["g"] = (uint8_t)(led_color>>8);
    color["b"] = (uint8_t)(led_color>>0);
  
    root["brightness"] = led_brightness;
    root["effect"] = effects[led_effect];
  }
  
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));
  Serial.print("[LED] SEND: "); 
  Serial.println(buffer);
  client.publish(mqtt_ledstrip_topic, buffer, true);
}

/** 
 * Kontrola, ci nebol zaznamenany pohyb, ak ano poslanie spravy na MQTT server
 */
void pir_check() {  
  long now_time = millis();
  
  int pir_value = digitalRead(PIR_PIN);
  
  if (pir_value != old_pir_value) {
    if(pir_value) {
      Serial.print("[PIR] SEND: ");
      Serial.println(mqtt_on);    
      client.publish(mqtt_pir_topic, mqtt_on);
    }
    else {
      Serial.print("[PIR] SEND: ");
      Serial.println(mqtt_off);
      client.publish(mqtt_pir_topic, mqtt_off);
    }
    old_pir_value = pir_value;
    last_msg_time = now_time;
  }
  else {
    if (now_time - last_msg_time > mqtt_keepalive) {  //kvoli keepalive preposielam hodnoty kazdych 5 sek.
      last_msg_time = now_time;
      if(old_pir_value) {
        client.publish(mqtt_pir_topic, mqtt_on);
      }
      else {
        client.publish(mqtt_pir_topic, mqtt_off);
      }
    }
  }  
}

/** 
 * Funkcia starajuca sa o flash - docasny alarm
 */
void flash_check() {
  if(flash) {
    if (flash_start_time<=0)
    {
      flash_start_time = millis();
      ws2812fx.setColor(flash_color);
      ws2812fx.setBrightness(flash_brightness);
      ws2812fx.setSpeed(flash_speed);
      ws2812fx.setMode(flash_effect);
    }    
    long now_time = millis();
    if ((now_time - flash_start_time) > flash_length) {
      flash = false;
      flash_effect = 23; //23==multi strobe
      flash_start_time = 0;
      update_led();
      led_send_state();
    }    
  }
}

/** 
 * Hlavna smycka
 */
void loop() {
  ws2812fx.service();

  if (WiFi.status() != WL_CONNECTED) {
    if(wifi_reconnect) {
      if(millis()-wifi_reconnect_start > WIFI_MAX_RECONNECT_TIME) {
        wifi_reconnect = false;
      }      
    }
    else {
      Serial.println("WIFI Disconnected. Attempting reconnection.");
      wifi_reconnect = true;
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      wifi_reconnect_start = millis();
      WiFi.begin(ssid, password);    
    }
    delay(1);
  }
  else {
    if(wifi_reconnect) {
      wifi_reconnect = false;
      Serial.println("WIFI Reconnected.");
    }
    
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    flash_check();
    pir_check();
  }
}

