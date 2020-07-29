
// Including the ESP8266 WiFi library
#include <ESP8266WiFi.h>

// Including libraryes for Real Time Clock. https://lastminuteengineers.com/ds1307-rtc-arduino-tutorial/
#include <Wire.h>
#include "RTClib.h"

#define TIME_VALVES_SHEDULE_CHECK_PERIOD 1000   // Period of checking if valves need to be open (* 10 ms)
#define TIME_SERIAL_PERIOD 5   // Period of handling serial comands (* 10 ms)
unsigned long timing; // 
unsigned int valvesShaduleCheckCount; // Preiod counter for program interaption of checking valves shadule
unsigned int serialPortTimeCount; // Preiod counter for program interaption of serial port listening

// Initiating instance for RTC
RTC_DS1307 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

const int CLOCK_GND = 16;



// Network details
const char* ssid = "YourSSID";
const char* password = "YourPass";

IPAddress staticIp(192,168,1,136);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

IPAddress localStaticIp(192,168,1,136);
IPAddress localGateway(192,168,1,136);
IPAddress localSubnet(255,255,255,0);

// Set web server port number to 80
WiFiServer server(80);
WiFiClient client;

// Variables to store the HTTP request
char mode=0;  // logical step of http request processing
char tempChar;
unsigned int ind;
char headContentLength[]= "Content-Length";
char firstLetter; // First letter of request
// todo make requestMethod as a char[]
String requestMethod;
boolean equalLetter;
char urnFromRequest[51];
boolean urnReceived= false;
unsigned int indUrn;  // index of URN string
unsigned int contentLength= 0;  
boolean emptyLine;  // empty line flag
char dataPost[201];  // string of data from POST-request
char par[4];
boolean requestRecieved=false;  // Is request recived flag

// Class Valve
class Valve {
  public:
    bool openState = false;                  // Current valve status 
    String openStateName = "Close";          // Name of current valve status
    bool manualyOpened = false;          // Is valve opened manualy
    bool onScheduleOpening = true;          // Should valve be opend by time?
    unsigned int openOnceANumberDays = 1;    // Once what number of days valve should be opened
    uint8_t openHour  = 7;                   // When valve should be opened in houres
    uint8_t openMin  = 0;                    // When valve should be opened in minutes
    uint8_t closeHour  = 7;                  // When valve should be closed in houres
    uint8_t closeMin  = 10;                  // When valve should be closed in minutes
    DateTime lastTimeOpened;                 // When valve should be closed in minutes

    Valve(byte pin);
    void openValve();
    void manualyOpenValve();
    void closeValve();
    void setOpenTime(uint8_t oHour, uint8_t oMin, uint8_t cHour, uint8_t cMin, unsigned int openOnceADays);
    bool CheckIfNeedToBeOpenedNow(DateTime now);

  private:
    byte _pin;                          // What GPIO pin assign to valve 
};

//// Assign output variables to GPIO pins
byte VALVE_1_PIN  = 12;
byte VALVE_2_PIN  = 14;
 
Valve valve1(VALVE_1_PIN);
Valve valve2(VALVE_2_PIN);


void setup() {
  Serial.begin(115200);

  // If not set custom ground, clock will not work, also for resetting if it is stuck.
  pinMode(CLOCK_GND, OUTPUT);
  digitalWrite(CLOCK_GND, LOW);

  // Connect to Wi-Fi network with SSID and password
  WiFi.mode(WIFI_AP_STA);
  // Station Mode Configuration
  Serial.printf("Connecting to %s\n", ssid);
  
  /*
  WiFi.begin(ssid, password);
  // WiFi.config(staticIp, gateway, subnet);

  // Trying to connect to WiFi
  for(int i=1; i<=10 ; i++) 
  {
    if(WiFi.status() == WL_CONNECTED){
        // Print local IP address 
        Serial.println();
        Serial.print("Connected, IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("MAC Address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("Gateway IP: ");
        Serial.println(WiFi.gatewayIP());
        Serial.print("DNS Server: ");
        Serial.println(WiFi.dnsIP());
        break;
    }
    delay(500);
    Serial.print(".");
  }
*/
    // SoftAP configuration
    // if(WiFi.status() != WL_CONNECTED){
    // Serial.print("Setting soft-AP configuration ... ");
    // Serial.println(WiFi.softAPConfig(localStaticIp, localGateway, localSubnet) ? "Ready" : "Failed!");
    Serial.println("Setting soft-AP ... ");
    Serial.println(WiFi.softAP("ESPsoftAP", password) ? "Ready" : "Failed!");
    Serial.print("Soft-AP IP address = ");
    Serial.println(WiFi.softAPIP());
 // }

   // Connecting to real time clock
  if(!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC lost power, lets set the time!");
  
    // Comment out below lines once you set the date & time.
    // Following line sets the RTC to the date & time this sketch was compiled
    
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // printDatetimeInConsole(rtc.now());
  
    // Following line sets the RTC with an explicit date & time
    // for example to set January 27 2017 at 12:56 you would call:
    // rtc.adjust(DateTime(2017, 1, 27, 12, 56, 0));
  }
  
  server.begin();
}

void loop(){
  timerInterupt();
  if(valvesShaduleCheckCount >= TIME_VALVES_SHEDULE_CHECK_PERIOD){
      valvesShaduleCheckCount = 0;
      checkShaduleOfOpeningValves();
  }
  if(serialPortTimeCount >= TIME_SERIAL_PERIOD){
      serialPortTimeCount = 0;
      handleSerial();
  }
  handleWebServer();
}

// Programmable interruption implemented to distribute resources of the controller execution time.
void  timerInterupt() {
  // Checking if difference of milliseconds from system start 
  // and last saved amount of milliseconds from system start  
  // is greater then 10 ms
 if(millis() - timing > 10){
  timing = millis();
  valvesShaduleCheckCount++;  // counter of period of checking valves opening shadule
  serialPortTimeCount++;  // counter of period of serial port handling  
 } 
}

  Valve::Valve(byte pin){
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, HIGH);
  }
  void Valve::openValve(){
    digitalWrite(_pin, LOW); // Set outputs to LOW because relays is fiaring on LOW lavel.
    openStateName = "Open";
    openState = true;
    lastTimeOpened = rtc.now();
  }
  void Valve::manualyOpenValve(){
    openValve();
    manualyOpened = true;
  }
  void Valve::closeValve(){
    digitalWrite(_pin, HIGH); // Set outputs to HIGH because relays switching of on HIGH lavel.
    openStateName = "Close";
    openState = false;
    manualyOpened = false;
  }
  void Valve::setOpenTime(
    uint8_t oHour,
    uint8_t oMin,
    uint8_t cHour,
    uint8_t cMin,
    unsigned int  openOnceADays){
    onScheduleOpening = true;          // Should valve be opend by time?
    openHour  = oHour;                  // When valve should be opened in houres
    openMin  = oMin;                    // When valve should be opened in minutes
    closeHour  = cHour;                // When valve should be closed in houres
    closeMin  = cMin;                  // When valve should be closed in minutes
    openOnceANumberDays = openOnceADays;
  }
  bool Valve::CheckIfNeedToBeOpenedNow(DateTime now){
    
    if(manualyOpened){
      return true;  
    }
    else if(!onScheduleOpening){
      return false;
    }
    else if(
      // The valve should open according to the schedule
      (DateTime(now.year(), now.month(), now.day(), openHour, openMin, 0) < now) && 
      (now < DateTime(now.year(), now.month(), now.day(), closeHour, closeMin, 0)) && 
      // and (if the configured number of days has passed since the last time or it is already open).
      ((now - lastTimeOpened).days() >= openOnceANumberDays || openState) 
    ){
      return true;
    }
  }

void checkShaduleOfOpeningValves(){
  DateTime now = rtc.now();
  if(valve1.CheckIfNeedToBeOpenedNow(now)){
    valve1.openValve();
  } else {valve1.closeValve();}
  if(valve2.CheckIfNeedToBeOpenedNow(now)){
    valve2.openValve();
  } else {valve2.closeValve();}
}

void print_datetime_in_console(DateTime date){
  
  Serial.print("Date & Time: ");
    Serial.printf(
      "%d/%d/%d (%s) %d:%d:%d",
      date.year(), 
      date.month(), 
      date.day(), 
      daysOfTheWeek[date.dayOfTheWeek()],
      date.hour(),
      date.minute(),
      date.second()
    );
    Serial.println();  
}

String StringFromDatetime(DateTime date){
  String stringifiedDate = 
      String(date.year()) + '/' +
      String(date.month()) + '/' +
      String(date.day()) + ' ' +
      // String(daysOfTheWeek[date.dayOfTheWeek()]) + ' ' +
      String(date.hour()) + ':' +
      String(date.minute()) + ':' +
      String(date.second()) ;
  return stringifiedDate;
}

void handleSerial(){
   // Handling serial commands 
    if (Serial.available() > 0) {
    String str = Serial.readStringUntil('\n');
    if (str == "show time") {
      printDatetimeInConsole(rtc.now());
      
    }
    if (str == "show datetime") {
      Serial.println(StringFromDatetime(rtc.now()));  
    }
    if (str == "set time") {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      printDatetimeInConsole(rtc.now());
    }
    if (str == "begin") {
      Serial.print("begining: ");
      Serial.println(rtc.begin());
      printDatetimeInConsole(rtc.now());
    }
    if (str == "isrunning") {
      Serial.print("is running: ");
      Serial.println(rtc.isrunning());
      printDatetimeInConsole(rtc.now());
    }
    if (str == "reset clock") {
      Serial.print("resetting clock: ");
      digitalWrite(CLOCK_GND, HIGH);
      delay(500);
      digitalWrite(CLOCK_GND, LOW);
      printDatetimeInConsole(rtc.now());
    }
    if (str == "stopWebRequest") {
      Serial.print("stop");
      mode = 30;     
    }
    if (str == "valve") {
      Serial.println("Valve 1:");
      Serial.print(valve1.openState);
      Serial.println(valve1.openStateName);
      Serial.print("onScheduleOpening: ");  
      Serial.println(valve1.onScheduleOpening);
      Serial.print("Once a number days ");  
      Serial.println(valve1.openOnceANumberDays);
      Serial.print("Open by time: ");
      Serial.println(valve1.openStateName);
      Serial.print("Open:");
      Serial.print(valve1.openHour);
      Serial.print(":");
      Serial.println(valve1.openMin);
      Serial.print("Close:");
      Serial.print(valve1.closeHour);
      Serial.print(":");
      Serial.println(valve1.closeMin);
      Serial.print(valve1.lastTimeOpened.day());
      Serial.print("/");
      Serial.print(valve1.lastTimeOpened.month());
      Serial.print("/");
      Serial.println(valve1.lastTimeOpened.year());
    }
    if (str == "valve/open") {
      valve1.openValve();
      Serial.println("Valve 1 opened");
    }
    if (str == "valve/close") {
      valve1.closeValve();
      Serial.println("Valve 1 closed");
    }
    if (str == "valve/needOpen") {
      Serial.print("Valve need to be opend?: ");
      Serial.println(valve1.CheckIfNeedToBeOpenedNow( rtc.now() ));
    }
    if (str == "valve/setTime") {
      Serial.println("Set time to open.");
      valve1.setOpenTime(17, 30, 20, 30, 1);
    }
    if (str == "valve/setYes") {
      Serial.println("Set yasterday as last time opend for test .");
      valve1.lastTimeOpened = DateTime(20, 7, 19, 0, 0, 0);
    } 
  }  
}

void handleWebServer(){

  // Handling http requests
  //------------------- wating for client 
  if( mode == 0 ) { 
    client = server.available(); // initializing client object
    if (client) {
      Serial.println(F("New request from client:"));
      ind=0;
      requestMethod = "None";
      equalLetter=true;
      emptyLine= true;
      urnReceived = false;
      indUrn=0xffff;
      contentLength= 0;
      mode=1;
    }
  }
 //-------------------- reading request method
  else if (mode == 1){
    if( ! client.connected() ) mode= 30; // disconnect
    if( client.available() ) {
      tempChar = client.read();
      // 
      if( tempChar == 'G' ) {
        requestMethod = "GET";
        Serial.println(F("GET-request"));
        Serial.write(tempChar);
        mode= 2; // reading URN
        }
        else if( tempChar == 'P' ) {
          requestMethod = "POST";
          Serial.print(F("POST-request"));
          Serial.write(tempChar);
          mode= 2; // reading URN
        }
        else {
          // this request has error, if there is no allowed requests methods
          mode= 40; // error handling
       }                       
    }
    
  }
  //------------------- reading URN
  else if( mode == 2 ) {
    if( ! client.connected() ) mode= 30; // Disconnect
    if (client.available()) {
        tempChar = client.read();
        Serial.write(tempChar);
        if( urnReceived == false ) {
          
          if( indUrn == 0xffff ) {
            // skipping the request method until URN begins 
            if( tempChar == '/' ) indUrn=0;  
          }
          else {
            // string writing
            if( tempChar == ' ' ) {
              // URN is ended
              urnFromRequest[indUrn]=0;
              urnReceived = true;
              if (requestMethod == "GET"){
                mode = 3;            
              } else {
                mode = 4;
              }             
            }
            else {
              // writing URN letter to the string
              urnFromRequest[indUrn] = tempChar;
              indUrn++;
              if( indUrn > 49 ) {
                // Overflow
                urnFromRequest[50]=0;
                urnReceived = true;
                if (requestMethod == "GET"){
                    mode = 3;            
                } else {
                    mode = 4;
                }
              }
            }            
          }          
        }
      }   
  }

  //------------------- processing GET-request
  else if(mode == 3) {
            Serial.print("GET processing");
            // turns the GPIOs on and off
            if ( strcmp(urnFromRequest, "valve1/on") == 0) {
              Serial.println("Valve 1 on");
              valve1.manualyOpenValve();
            } else if ( strcmp(urnFromRequest, "valve1/off") == 0) {
              Serial.println("Valve 1 off");
              valve1.closeValve();
            } else if ( strcmp(urnFromRequest, "valve2/on") == 0) {
              Serial.println("Valve 2 on");
              valve2.manualyOpenValve();
            } else if ( strcmp(urnFromRequest, "valve2/off") == 0) {
              Serial.println("Valve 2 off");
              valve2.closeValve();
            } else if(strcmp(urnFromRequest, "valve1/onScheduleOpening/on") == 0) {
              Serial.println("Valve 1 on shadule open");
              valve1.onScheduleOpening = true;
            } else if(strcmp(urnFromRequest, "valve1/onScheduleOpening/off") == 0) {
              Serial.println("Valve 1 desabled on shadule open");
              valve1.onScheduleOpening = false;
            } else if(strcmp(urnFromRequest, "valve2/onScheduleOpening/on") == 0) {
              Serial.println("Valve 2 on shadule open");
              valve2.onScheduleOpening = true;
            } else if(strcmp(urnFromRequest, "valve2/onScheduleOpening/off") == 0) {
              Serial.println("Valve 2 desabled on shadule open");
              valve2.onScheduleOpening = false;
            } else if ( strcmp(urnFromRequest, "clock/reset") == 0) {
              Serial.println("Clock reset");
              digitalWrite(CLOCK_GND, HIGH);
              delay(100);
              digitalWrite(CLOCK_GND, LOW);
            }
            mode = 8;
  }
  
  //------------------- searching for headers
  else if( mode == 4) {

    if( ! client.connected() ) mode= 30; // Breaking connection
    if( client.available() ) {
      tempChar = client.read();
      Serial.write(tempChar);

      if( tempChar != '\r' && tempChar != '\n' ) emptyLine= false; // Indicator of an empty string  

      if( tempChar == ':' ) {
        // end of header, checking
        if( equalLetter == true ) {
          // All characters are the same
          ind=0;
          mode=5; // Switching to reading a parameter
        }
      }
      else {
        // Comparison of characters of header
        if( tempChar != headContentLength[ind] ) equalLetter= false; 
        ind++;        
      }

      if( tempChar == '\n' ) {
        // new line
        if( emptyLine == true ) {
          // empty line, headars is ending
          Serial.print("request body:");
          ind=0;
          mode=6; 
        }
        else {
          // The headers havn't ended
          ind=0;
          equalLetter=true;
          emptyLine= true;
        }        
      }             
    }    
  }

  //------------------- reading parameter of the Content-Length header
  else if( mode == 5) {

    if( ! client.connected() ) mode= 30; //  Disconnect
    if( client.available() ) {
      tempChar = client.read();
      Serial.write(tempChar);

      if( tempChar == '\n' ) {
        // конец строки
        par[ind]=0;        
        contentLength= atoi(par); // converting to a number
        ind=0;
        equalLetter=true;
        emptyLine= true;        
        mode=4;
        if( contentLength == 0 ) mode= 40; // error handling
      }
      else {
        if( tempChar != ' ' && tempChar != '\r' ) {
          
          Serial.write(tempChar);
          par[ind]= tempChar;
          ind++; if( ind > 3 ) ind=0;  
        }        
      }      
    }    
  }

  //------------------- receiving POST data
  else if( mode == 6) {
    if( ! client.connected() ) mode= 30; //  Disconnect
    if( client.available() ) {
      tempChar = client.read();
      Serial.write("Reading post data: ");
      Serial.write(tempChar);
      if(tempChar != 0 
        && tempChar != ' ' 
        && tempChar != '\r'
        && tempChar != '\n'
        && tempChar != '\0'
        && tempChar != '\t'
        && tempChar != '\f'
        && tempChar != '\e') {
        dataPost[ind]= tempChar;
        ind++;
      }
      contentLength--;
      if( contentLength == 0 ) {
        // все принято
        dataPost[ind]='\0'; // setting the end of the string when data from request is received
        
        // todo post request answer
        //client.println("HTTP/1.1 200 OK"); 
        //client.println("Content-type:text/html");
        //client.println("Connection: close");
        //client.println(); 
        // // Display the HTML web page
        //client.println("<!DOCTYPE html><html>");
        //client.println("<head><meta http-equiv=\"refresh\" content=\"0\"; url=\"/\"></head></html>");
        //client.println(); 
             
        mode= 7; 
      }      
    }    
  }
  //------------------- data process
  else if( mode == 7) {
    bool writing_param_key = true; 
    unsigned int parce_ind = 0; // Index of processed letter in string of parced param key or value. 
    char PostParamKey[30];  // Temp array for key string of the param in POST request
    char PostParamValue[10];  //  Temp array for value string of the param in POST request
    
    Serial.println();
    Serial.print("Starting data processing. Array len:");
    Serial.println(strlen(dataPost));
    String post_data(dataPost);
    Serial.println(post_data);
    
    for(int i=0; i<200; i++ ) {
      if( dataPost[i] == '=' ){
        Serial.write(dataPost[i]);
        Serial.println(" Start writing value");
        writing_param_key = false;
        PostParamKey[parce_ind]= '\0'; // Setting end of string, so the new data will not be messed with the old one.
        parce_ind = 0; 
        continue;
      } 
      if(dataPost[i] == '&'|| dataPost[i] == '\n' || dataPost[i] == '\0'){
        Serial.println("Saving value");
        PostParamValue[parce_ind]= '\0'; // Setting end of string, so the new data will not be messed with the old one.
        
        // todo : make shadule saving with Valve::setOpenTime
        if(strcmp(urnFromRequest, "valve/1/setShaduleTime") == 0){
          // Setting shadule for valve 1
          if(strcmp( PostParamKey, "valve1openHour") == 0)valve1.openHour= atoi(PostParamValue);
          if(strcmp( PostParamKey, "valve1openMin") == 0) valve1.openMin= atoi(PostParamValue);
          if(strcmp( PostParamKey, "valve1closeHour") == 0)valve1.closeHour= atoi(PostParamValue);
          if(strcmp( PostParamKey, "valve1closeMin") == 0) valve1.closeMin= atoi(PostParamValue);
          if(strcmp( PostParamKey, "valve1openOnceANumberDays") == 0) valve1.openOnceANumberDays= atoi(PostParamValue);
        }
        if(strcmp(urnFromRequest, "valve/2/setShaduleTime") == 0){
          // Setting shadule for valve 2
          if(strcmp( PostParamKey, "valve2openHour") == 0)valve2.openHour= atoi(PostParamValue);
          if(strcmp( PostParamKey, "valve2openMin") == 0) valve2.openMin= atoi(PostParamValue);
          if(strcmp( PostParamKey, "valve2closeHour") == 0)valve2.closeHour= atoi(PostParamValue);
          if(strcmp( PostParamKey, "valve2closeMin") == 0) valve2.closeMin= atoi(PostParamValue);
          if(strcmp( PostParamKey, "valve2openOnceANumberDays") == 0) valve2.openOnceANumberDays= atoi(PostParamValue);
        }

        // Setting current time
        if(strcmp(urnFromRequest, "setCurrentTime") == 0){

          if(strcmp( PostParamKey, "currentHour") == 0){
            DateTime now = rtc.now();
            Serial.println("Saving hour");
            printDatetimeInConsole(DateTime(now.year(), now.month(), now.day(), atoi(PostParamValue), now.minute(), 0));
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), atoi(PostParamValue), now.minute(), 0));
          }
          if(strcmp( PostParamKey, "currentMin") == 0){
            DateTime now = rtc.now();
            Serial.println("Saving minutes");
            printDatetimeInConsole(DateTime(now.year(), now.month(), now.day(), now.hour(), atoi(PostParamValue), 0));
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), atoi(PostParamValue), 0));
          }
        } 
         // Setting current Date
         if(strcmp(urnFromRequest, "setCurrentDate") == 0){
            if(strcmp( PostParamKey, "currentDate") == 0){
              DateTime now = rtc.now();
              Serial.println("Saving date");
              printDatetimeInConsole(DateTime(now.year(), now.month(), atoi(PostParamValue), now.hour(), now.minute(), 0));
              rtc.adjust(DateTime(now.year(), now.month(), atoi(PostParamValue), now.hour(), now.minute(), 0));
              
            }
            if(strcmp( PostParamKey, "currentMonth") == 0){
              DateTime now = rtc.now();
              Serial.println("Saving month");
              printDatetimeInConsole(DateTime(now.year(), atoi(PostParamValue), now.day(), now.hour(),now.minute(), 0));
              rtc.adjust(DateTime(now.year(), atoi(PostParamValue), now.day(), now.hour(),now.minute(), 0));
            }
            if(strcmp( PostParamKey, "currentYear") == 0){
              DateTime now = rtc.now();
              Serial.println("Saving year");
              printDatetimeInConsole(DateTime(atoi(PostParamValue), now.month(), now.day(), now.hour(), now.minute(), 0));
              rtc.adjust(DateTime(atoi(PostParamValue), now.month(), now.day(), now.hour(), now.minute(), 0));
            }
         }
         
        writing_param_key = true;
        parce_ind = 0; // empting index for new parameter key  
        if (dataPost[i] == '\0'){
          break;
        } else {
          continue;
        }
      }
      if(writing_param_key){
        Serial.write(dataPost[i]);
        Serial.println(" Writing key");
        PostParamKey[parce_ind] = dataPost[i];
        parce_ind++;
        continue;
      }
      if(!writing_param_key){
        Serial.write(dataPost[i]);
        Serial.println(" Writing value");
        PostParamValue[parce_ind] = dataPost[i];
        parce_ind++;
        continue;
      }
    }
    Serial.println("PostParams prosessed");
    requestRecieved=true;
    mode = 8;
  }
  //------------------- returning Page
  else if (mode == 8) {
            
    renderWebPage();
    mode= 30; // Disconnect
  }
  //------------------- handling error request
  else if( mode == 40) {

    Serial.println(F("ERROR-request"));

    // ответ клиенту
    client.println(F("HTTP/1.1 400"));
    client.println(); // a blank line separates the body of the message        
    
    mode= 30; // Breaking connection
  }
  //------------------- breaking connection
  else if( mode == 30) {
    delay(1);    
    client.stop();  // Breaking connection
    Serial.println("Break");
    mode=0;
  }
  else mode=0;
  // Checking a request attribute
  if( requestRecieved == true ) {
    requestRecieved= false;
  }
  
}

void renderWebPage(){
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line:
  DateTime now = rtc.now();
  Serial.print("Page rendering");
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println(); 
            
  // Display the HTML web page
  client.println("<!DOCTYPE html><html>");
  client.println("<head><meta name=\"viewport\" charset=\"utf-8\" content=\"width=device-width, initial-scale=1\">");
  
  // CSS to style the on/off buttons 
  // Feel free to change the background-color and font-size attributes to fit your preferences
  client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
  client.println("div {flex-direction: row;}");
  client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;}");
  client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
  client.println(".button2 {background-color: #77878A;}</style></head>");
  
  // Web Page Heading
  client.println("<body><h1>Greenhouse Web Server</h1>");
  client.print("<p>Current time: "); 
  client.print(now.hour()); 
  client.println(":");
  client.print(now.minute());
  client.println("</p>");
   
  // Display current state, and ON/OFF buttons for Valve 1 
  client.println("<p>Valve 1 - State " + valve1.openStateName + "</p>");
  client.println("<p>Last time opened: "+ StringFromDatetime( valve1.lastTimeOpened) +"</p>");
  client.print("<div>");
  // If the valve_1_state is off, it displays the ON button 
  if (valve1.openState == false) {
    client.println("<p><a href=\"/valve1/on\"><button class=\"button\">Open</button></a></p>");
  } else {
    client.println("<p><a href=\"/valve1/off\"><button class=\"button button2\">Close</button></a></p>");
  } 
  if (valve1.onScheduleOpening == false) {
    client.println("<p><a href=\"/valve1/onScheduleOpening/on\"><button class=\"button\">Enable </button></a></p>");
  } else {
    client.println("<p><a href=\"/valve1/onScheduleOpening/off\"><button class=\"button button2\">Disable</button></a></p>");
  } 
  client.print("</div>");
  
  client.println("<form method=\"post\" action=\"/valve/1/setShaduleTime\">");
  client.println("<p>Opening time:</p>");
  client.print("<div>"); 
  client.print("<input type=\"number\"  min=\"0\" max=\"24\" value=\""); 
  client.print(valve1.openHour); 
  client.print("\" name=valve1openHour >");
  client.print("<input type=\"number\" min=\"0\" max=\"60\" value=\""); 
  client.print(valve1.openMin); 
  client.print("\" name=valve1openMin >");
  client.print("</div>");
  client.println("<p>Closing time:</p>");
  client.print("<div>"); 
  client.print("<input type=\"number\"  min=\"0\" max=\"24\" value=\""); 
  client.print(valve1.closeHour); 
  client.print("\" name=valve1closeHour >");
  client.print("<input type=\"number\" min=\"0\" max=\"60\" value=\""); 
  client.print(valve1.closeMin); 
  client.print("\" name=valve1closeMin >");
  client.print("</div>");
  client.println("<p>Once in how many days:</p>");
  client.print("<input type=\"number\" min=\"0\" max=\"365\" value=\""); 
  client.print(valve1.openOnceANumberDays); 
  client.print("\" name=valve1openOnceANumberDays >");
  
  client.println("<input type=\"submit\" value=\"Submit\" >");
  client.println("</form>");
  
   // Display current state, and ON/OFF buttons for Valve 2 
  client.println("<p>Valve 2 - State " +  valve2.openStateName  + "</p>");
  client.println("<p>Last time opened: "+ StringFromDatetime( valve2.lastTimeOpened) +"</p>");
  // If the valve_2_state is off, it displays the ON button       
  client.print("<div>");
  if (valve2.openState == false) {
    client.println("<p><a href=\"/valve2/on\"><button class=\"button\">Open</button></a></p>");
  } else {
    client.println("<p><a href=\"/valve2/off\"><button class=\"button button2\">Close</button></a></p>");
  } 
  if (valve2.onScheduleOpening == false) {
    client.println("<p><a href=\"/valve2/onScheduleOpening/on\"><button class=\"button\">Enable </button></a></p>");
  } else {
    client.println("<p><a href=\"/valve2/onScheduleOpening/off\"><button class=\"button button2\">Disable</button></a></p>");
  } 
  client.print("</div>");
  
  client.println("<form method=\"post\" action=\"/valve/2/setShaduleTime\">");
  client.println("<p>Opening time:</p>");
  client.print("<div>"); 
  client.print("<input type=\"number\"  min=\"0\" max=\"24\" value=\""); 
  client.print(valve2.openHour); 
  client.print("\" name=valve2openHour >");
  client.print("<input type=\"number\" min=\"0\" max=\"60\" value=\""); 
  client.print(valve2.openMin); 
  client.print("\" name=valve2openMin >");
  client.print("</div>");
  client.println("<p>Closing time:</p>");
  client.print("<div>"); 
  client.print("<input type=\"number\"  min=\"0\" max=\"24\" value=\""); 
  client.print(valve2.closeHour); 
  client.print("\" name=valve2closeHour >");
  client.print("<input type=\"number\" min=\"0\" max=\"60\" value=\""); 
  client.print(valve2.closeMin); 
  client.print("\" name=valve2closeMin >");
  client.print("</div>");
  client.println("<p>Once in how many days:</p>");
  client.print("<input type=\"number\" min=\"0\" max=\"365\" value=\""); 
  client.print(valve2.openOnceANumberDays); 
  client.print("\" name=valve2openOnceANumberDays >");
  client.println("<input type=\"submit\" value=\"Submit\" >");
  client.println("</form>");

  client.println("<form method=\"post\" action=\"/setCurrentTime\">");
  client.println("<p>Set current time:</p>");
  client.print("<div>"); 
  client.print("<input type=\"number\"  min=\"0\" max=\"24\" value=\""); 
  client.print(now.hour()); 
  client.print("\" name=currentHour >");
  client.print("<input type=\"number\" min=\"0\" max=\"60\" value=\""); 
  client.print(now.minute()); 
  client.print("\" name=currentMin >");
  client.println("<input type=\"submit\" value=\"Submit\" >");
  client.print("</div>");
  client.println("</form>");

  client.println("<form method=\"post\" action=\"/setCurrentDate\">");
  client.println("<p>Set current date:</p>");
  client.print("<div>"); 
  client.print("<input type=\"number\"  min=\"0\" max=\"31\" value=\""); 
  client.print(now.day()); 
  client.print("\" name=currentDate >");
  client.print("<input type=\"number\" min=\"0\" max=\"12\" value=\""); 
  client.print(now.month()); 
  client.print("\" name=currentMonth >");
  client.print("<input type=\"number\" min=\"0\" max=\"5001\" value=\""); 
  client.print(now.year()); 
  client.print("\" name=currentYear >");
  client.println("<input type=\"submit\" value=\"Submit\" >");
  client.print("</div>");
  
  client.println("</form>");

  client.println("</body></html>");
  
  // The HTTP response ends with another blank line
  client.println();
}
