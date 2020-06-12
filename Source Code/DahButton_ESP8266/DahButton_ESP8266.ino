/*
 *  Web Interfacing Program for Dah Button ESP
 * 
 *  Company:    Naked Ninja (c) 2020
 *  Website:    https://nakedninja.cc
 *  Author(s):  Caner Erdem & Harm Verbeek
 *  
 */

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// Deze pinnen checken op de PCB!!!
#define SHUTDOWN_PIN  4  // GPIO4
#define SWITCH_PIN    5  // GPIO5
#define ACT_LED_PIN   13  // GPIO6 - RGB Led

#define R1   4700
#define R2   1000 
#define VREF 1.0

#define LED_OK      1 // Green
#define LED_WARNING 2 // Yellow
#define LED_ERROR   3 // Red

#define USE_SERIAL Serial

Adafruit_NeoPixel pixel(1, ACT_LED_PIN, NEO_GRB + NEO_KHZ800);

char url[80]   = "http://yourserver.com/set?id=";
char value[50] = "1";
char protocol[8] = "GET"; // GET, PUT, POST, PATCH, DELETE
char auth_user[10] = "uer"; // Basic Authentication User
char auth_password[10] = "user"; // Basic Authentication Password
char rest_string[120] = "";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

float get_battery_voltage() 
{
  int adc_value = analogRead(0);
  float battery_voltage = VREF / 1024.0 * adc_value;
  battery_voltage = battery_voltage * (R1 + R2) / R2;
  
  return battery_voltage;
}


void vreg_shutdown() 
{
  USE_SERIAL.println("\nSignal the ATtiny to turn off the voltage regulator...");
  
  pinMode(SHUTDOWN_PIN, OUTPUT);
  digitalWrite(SHUTDOWN_PIN, HIGH);
  delay(50);  
  digitalWrite(SHUTDOWN_PIN, LOW);
  
  delay(50);  
  ESP.deepSleep(0);
  yield();

  //while (true) {
  //  delay(5);
  //}
}

/*
 * Colors:
 * 
 * (0, 0, 127)     // 1/4 bright blue
 * (0, 63, 0)      // 1/4 bright green
 * (255, 0, 0)     // full-bright red 
 * (0, 255, 255)   // full-bright cyan
 * (127, 127, 0)   // half-bright yellow
 * (255, 192, 255) // orange
 * (63, 63, 63)    // 1/4-bright white
 */
void blink(int mode, int blink_count)
{
  switch (mode) {
    case LED_OK:
      for (int i = 0; i < blink_count; i++) {
        // pixel.Color takes RGB values, from 0,0,0 up to 255,255,255
        pixel.setPixelColor(0, pixel.Color(0,127,0)); // Moderately bright green color.
        pixel.show(); // This sends the updated color to the hardware.
        delay(100);
        pixel.setPixelColor(0, pixel.Color(0,0,0)); // switch led off.
        pixel.show();
        delay(100);
      }
      break; 
    case LED_WARNING:
      for (int i = 0; i < blink_count; i++) {
        pixel.setPixelColor(0, pixel.Color(127,127,0)); // Moderately bright yellow color.
        pixel.show(); // This sends the updated color to the hardware.
        delay(100);
        pixel.setPixelColor(0, pixel.Color(0,0,0)); // switch led off.
        pixel.show();
        delay(100);
      }
      break; 
    case LED_ERROR:
      for (int i = 0; i < blink_count; i++) {
        pixel.setPixelColor(0, pixel.Color(127,0,0)); // Moderately bright red color.
        pixel.show(); // This sends the updated color to the hardware.
        delay(100);
        pixel.setPixelColor(0, pixel.Color(0,0,0)); // switch led off.
        pixel.show();
        delay(100);
      }
      break; 
  }
}

String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
    
}

unsigned char h2int(char c)
{
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}

void setup() 
{
  // put your setup code here, to run once:
  USE_SERIAL.begin(115200);
  USE_SERIAL.println("\n Starting");

  // get and print the switch state
  int switch_state = digitalRead(SWITCH_PIN);
  USE_SERIAL.println("Switch state: " + String(switch_state));

  EEPROM.begin(512);
  delay(10);

  // turn off the activity led
  pixel.setPixelColor(0, pixel.Color(0,0,0));
  pixel.show();
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // initialize NeoPixel library
  pixel.begin(); 

  // get and print the battery voltage
  float vbat = get_battery_voltage();
  USE_SERIAL.println("Battery voltage: " + String(vbat) + "V");
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // read eeprom for status,url and value
  USE_SERIAL.println("Reading status from EEPROM");
  String my_status;
  for (int i = 0; i < 3; ++i) {
    my_status += char(EEPROM.read(i));
  }
  //USE_SERIAL.print("STATUS: ");
  USE_SERIAL.println(my_status);
  
  char my_url[80]   = "";
  char my_value[50] = "";
  char my_protocol[8] = ""; 
  char my_auth_user[10] = ""; 
  char my_auth_password[10] = ""; 
  char my_rest_string[120] = "";


  if (my_status == "ABC") { // EEPROM was written
    USE_SERIAL.println("Valid status found.");  

    USE_SERIAL.println("Reading url from EEPROM");
    for (int i = 10; i < 90; ++i) {
      if (EEPROM.read(i)!=0)
        my_url[i-10] += char(EEPROM.read(i));
    }
    USE_SERIAL.print("URL: ");
    USE_SERIAL.println(my_url);
    
    USE_SERIAL.println("Reading value from EEPROM");
    for (int i = 100; i < 150; ++i) {
      if (EEPROM.read(i)!=0)
        my_value[i-100] += char(EEPROM.read(i));
    }
    USE_SERIAL.print("VALUE: ");
    USE_SERIAL.println(my_value);  

    USE_SERIAL.println("Reading protocol from EEPROM");
    for (int i = 160; i < 168; ++i) {
      if (EEPROM.read(i)!=0)
        my_protocol[i-160] += char(EEPROM.read(i));
    }
    //USE_SERIAL.print("PROTOCOL: ");
    USE_SERIAL.println(my_protocol);

    USE_SERIAL.println("Reading Auth User from EEPROM");
    for (int i = 170; i < 180; ++i) {
      if (EEPROM.read(i)!=0)
        my_auth_user[i-170] += char(EEPROM.read(i));
    }
    USE_SERIAL.print("AUTH USER: ");
    USE_SERIAL.println(my_auth_user);

    USE_SERIAL.println("Reading Auth Password from EEPROM");
    for (int i = 190; i < 200; ++i) {
      if (EEPROM.read(i)!=0)
        my_auth_password[i-190] += char(EEPROM.read(i));
    }
    USE_SERIAL.print("AUTH PASSWORD: ");
    USE_SERIAL.println(my_auth_password);

    USE_SERIAL.println("Reading REST string from EEPROM");
    for (int i = 210; i < 330; ++i) {
      if (EEPROM.read(i)!=0)
        my_rest_string[i-210] += char(EEPROM.read(i));
    }
    USE_SERIAL.print("REST_STRING: ");
    USE_SERIAL.println(my_rest_string);  
  }
  else {
    // use default values
    strcpy(my_url, url);
    strcpy(my_value, value);
    strcpy(my_protocol, protocol);
    strcpy(my_auth_user, auth_user);
    strcpy(my_auth_password, auth_password);
    strcpy(my_rest_string, rest_string);
  }
  
  // is configuration portal requested?
  if ( switch_state == HIGH ) {
    //reset settings - for testing
    //wifiManager.resetSettings();
    
    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep
    //in seconds
    wifiManager.setTimeout(180); // 3 minutes

    //it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration

    //WITHOUT THIS THE AP DOES NOT SEEM TO WORK PROPERLY WITH SDK 1.5 , update to at least 1.5.1
    //WiFi.mode(WIFI_STA);

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    
    // Exit after saving configuration
    wifiManager.setBreakAfterConfig(true);

    // id/name, placeholder/prompt, default, length
    WiFiManagerParameter custom_url("URL", "url", my_url, 80);
    wifiManager.addParameter(&custom_url);

    WiFiManagerParameter custom_value("Value", "value", my_value, 50);
    wifiManager.addParameter(&custom_value);

    WiFiManagerParameter custom_protocol("Protocol", "protocol", my_protocol, 8);
    wifiManager.addParameter(&custom_protocol);

    WiFiManagerParameter custom_auth_user("AUTH USER", "user", my_auth_user, 10);
    wifiManager.addParameter(&custom_auth_user);

    WiFiManagerParameter custom_auth_password("AUTH PASSWORD", "password", my_auth_password, 10);
    wifiManager.addParameter(&custom_auth_password);

    // "{\"state\": \"[@]\", \"attributes\": {\"unit_of_measurement\": \"°C\", \"friendly_name\": \"Bathroom Temp\"}}"
    WiFiManagerParameter custom_rest_string("REST String", "", my_rest_string, 120);
    wifiManager.addParameter(&custom_rest_string);


    //start an access point with the specified name
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.startConfigPortal("DahButton1")) {
      USE_SERIAL.println("Failed to connect and hit timeout");

      if (shouldSaveConfig) {
        // If you get here you have connected to the WiFi
        USE_SERIAL.println("Connected and saving configuration ...");
    
        strcpy(my_url, custom_url.getValue());
        strcpy(my_value, custom_value.getValue());
        strcpy(my_protocol, custom_protocol.getValue());
        strcpy(my_auth_user, custom_auth_user.getValue());
        strcpy(my_auth_password, custom_auth_password.getValue());
        strcpy(my_rest_string, custom_rest_string.getValue());
    
        if (strlen(my_url) > 0) {
          //USE_SERIAL.println("clearing eeprom for url");
          for (int i = 10; i < 91; ++i) { EEPROM.write(10+i, 0); }
          //USE_SERIAL.println(my_url);
            
          USE_SERIAL.println("\nWriting url to eeprom:");
          for (int i = 0; i < strlen(my_url); ++i) {
              EEPROM.write(10+i, my_url[i]);
              //USE_SERIAL.print("Wrote: ");
              USE_SERIAL.print(my_url[i]); 
          }
        }
             
        if (strlen(my_value) > 0) {
          //USE_SERIAL.println("clearing eeprom for value");
          for (int i = 0; i < 51; ++i) { EEPROM.write(100+i, 0); }
          //USE_SERIAL.println(my_value);
            
          USE_SERIAL.println("\nWriting value to eeprom:");
          for (int i = 0; i < strlen(my_value); ++i) {
              EEPROM.write(100+i, my_value[i]);
              //USE_SERIAL.print("Wrote: ");
              USE_SERIAL.print(my_value[i]); 
          }
        }


        if (strlen(my_protocol) > 0) {
          //USE_SERIAL.println("clearing eeprom for protocol");
          for (int i = 0; i < 8; ++i) { EEPROM.write(160+i, 0); }
          //USE_SERIAL.println(my_protocol);
            
          USE_SERIAL.println("\nWriting value to eeprom:");
          for (int i = 0; i < strlen(my_protocol); ++i) {
              EEPROM.write(160+i, my_protocol[i]);
              //USE_SERIAL.print("Wrote: ");
              USE_SERIAL.print(my_protocol[i]); 
          }
        }

         if (strlen(my_auth_user) > 0) {
          //USE_SERIAL.println("clearing eeprom for auth user");
          for (int i = 0; i < 10; ++i) { EEPROM.write(170+i, 0); }
          //USE_SERIAL.println(my_auth_user);
            
          USE_SERIAL.println("\nWriting value to eeprom:");
          for (int i = 0; i < strlen(my_auth_user); ++i) {
              EEPROM.write(170+i, my_auth_user[i]);
              //USE_SERIAL.print("Wrote: ");
              USE_SERIAL.print(my_auth_user[i]); 
          }
        }

        if (strlen(my_auth_password) > 0) {
          //USE_SERIAL.println("clearing eeprom for auth password");
          for (int i = 0; i < 10; ++i) { EEPROM.write(190+i, 0); }
          //USE_SERIAL.println(my_auth_password);
            
          USE_SERIAL.println("\nWriting value to eeprom:");
          for (int i = 0; i < strlen(my_auth_password); ++i) {
              EEPROM.write(190+i, my_auth_password[i]);
              //USE_SERIAL.print("Wrote: ");
              USE_SERIAL.print(my_auth_password[i]); 
          }
        }

        if (strlen(my_rest_string) > 0) {
          //USE_SERIAL.println("clearing eeprom for rest string");
          for (int i = 0; i < 120; ++i) { EEPROM.write(210+i, 0); }
          //USE_SERIAL.println(my_rest_string);
            
          USE_SERIAL.println("\nWriting value to eeprom:");
          for (int i = 0; i < strlen(my_rest_string); ++i) {
              EEPROM.write(210+i, my_rest_string[i]);
              //USE_SERIAL.print("Wrote: ");
              USE_SERIAL.print(my_rest_string[i]); 
          }
        }
        
        // minimum requirements to save new values      
        if ((strlen(my_url) > 0)) {
          USE_SERIAL.println("\nWriting eeprom status:");
          EEPROM.write(0, 65); // A
          EEPROM.write(1, 66); // B
          EEPROM.write(2, 67); // C
          EEPROM.write(3, 0);
          USE_SERIAL.println("Wrote status");
          
          EEPROM.commit();
        }
    
        blink(LED_OK,1);
      }
           
    }

    // shut down
    vreg_shutdown();        
  }

  // USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();

  USE_SERIAL.printf("SSID: %s\n", WiFi.SSID().c_str());
  //USE_SERIAL.printf("PSK: %s\n", WiFi.psk().c_str());
  
  if (WiFi.SSID() == "" || WiFi.psk() == "") {
    // blink Error LED ...
    blink(LED_ERROR,2);
    
    // shut down
    vreg_shutdown();   
  }
   
  WiFi.begin();
  
  USE_SERIAL.print("[SETUP] WAIT ");
  int waitCounter = 0;
  while ((WiFi.status() != WL_CONNECTED) && (waitCounter++ < 12)) {
    USE_SERIAL.print(".");
    delay(1000);
  }
  USE_SERIAL.flush();

  // After trying to connect 10 times ...
  if (waitCounter == 12) {
    // blink Error LED ...
    blink(LED_ERROR,3);
    
    // shut down
    vreg_shutdown();        
  }
  
  WiFi.mode(WIFI_STA);

  HTTPClient http;
  char placeholder[121] = "";

  // to be replaced by auth_user and auth_password
  if (strlen(my_auth_user)>0 && strlen(my_auth_password)>0) {
//     http.setAuthorization("user", "user");
     http.setAuthorization(my_auth_user, my_auth_password);
  }

  //USE_SERIAL.println(strlen(my_url));
  //USE_SERIAL.println(my_url);
 
  // add value add the end of url string
  if (strlen(my_value) > 0)
     sprintf(placeholder,"%s%s",my_url, my_value);
  else   
    sprintf(placeholder,"%s",my_url);
  
  //USE_SERIAL.println(strlen(placeholder));

  //USE_SERIAL.print("\nmy_url: ");
  //USE_SERIAL.println(placeholder);

  USE_SERIAL.print("[HTTP] begin...\n");
  // configure secure server and url
  // http.begin("https://192.168.1.12/test.html", "7a 9c f4 db 40 d3 62 5a 6e 21 bc 5c cc 66 c8 3e a1 45 59 38"); //HTTPS
  // http.begin("http://www.google.nl/"); //HTTP
  http.begin(placeholder);
  int httpCode = 0;
  
  if (strcmp (my_protocol,"GET")==0) {
    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    httpCode = http.GET();
    
  } else if (strcmp(my_protocol,"POST")==0) {
    USE_SERIAL.print("[HTTP] POST...\n");
    // start connection and send HTTP header
    httpCode = http.POST("Hello World!"); 
       
  } else if (strcmp(my_protocol,"REST")==0) {
    USE_SERIAL.print("[HTTP] REST...\n");
    
    http.addHeader("x-ha-access", "apen00tz");
//    http.addHeader("x-ha-access", placeholder);
    http.addHeader("Content-Type", "application/json");

    int tC = random(12,30); 
    char bigBuf[100] = "";
//   my_rest_string.replace("[@]",String(tC));
//    my_rest_string = urlencode(my_rest_string);
    
    //my_rest_string.toCharArray(placeholder, my_rest_string.length());
    //placeholder[my_rest_string.length()] = '\0';
    Serial.print("placeholder: [");
//    Serial.print(my_rest_string);
//    Serial.println("]");
//    Serial.println(my_rest_string.length());
    
//   Serial.print("substring: [");
//    Serial.print(my_rest_string);
    Serial.println("]");
    const char *s = "{\"state\": \"%d\", \"attributes\": {\"unit_of_measurement\": \"°C\", \"friendly_name\": \"Bathroom Temp\"}}";
//    sprintf(bigBuf,"{\"state\": \"%d\", \"attributes\": {\"unit_of_measurement\": \"°C\", \"friendly_name\": \"Bathroom Temp\"}}",tC);
    sprintf(bigBuf,placeholder,tC);
    //httpCode = http.POST(String(bigBuf));
//    httpCode = http.POST(my_rest_string);
    //httpCode = http.POST(my_rest_string.substring(0,118));  
     
  } else {
    http.end();
    // shut down
    vreg_shutdown();
  }

  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    USE_SERIAL.printf("[HTTP] Result code: %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      USE_SERIAL.println(payload);
      
      blink(LED_OK,2);
    } else {
      blink(LED_ERROR,4);
    }
  } else {
    USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    
    // blink Error LED ...
    blink(LED_ERROR,5);
  }

  http.end();

  // shut down
  vreg_shutdown(); 
}


void loop() 
{
  yield();
}
