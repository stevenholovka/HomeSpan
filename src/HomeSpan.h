/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2020-2021 Gregg E. Berman
 *  
 *  https://github.com/HomeSpan/HomeSpan
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/
 
#pragma once

#ifndef ARDUINO_ARCH_ESP32
#error ERROR: HOMESPAN IS ONLY AVAILABLE FOR ESP32 MICROCONTROLLERS!
#endif

#include <Arduino.h>
#include <unordered_map>
#include <nvs.h>

#include "Settings.h"
#include "Utils.h"
#include "Network.h"
#include "HAPConstants.h"
#include "HapQR.h"
#include "Characteristics.h"

#include <esp_event.h>

using std::vector;
using std::unordered_map;

enum {
  GET_AID=1,
  GET_META=2,
  GET_PERMS=4,
  GET_TYPE=8,
  GET_EV=16,
  GET_DESC=32,
  GET_NV=64,
  GET_ALL=255
};

enum {
    HOMESPAN_WIFI_NEEDED,
    HOMESPAN_WIFI_CONNECTING,
    HOMESPAN_WIFI_CONNECTED,
    HOMESPAN_WIFI_DISCONNECTED,
    HOMESPAN_AP_STARTED,
    HOMESPAN_AP_CONNECTED,
    HOMESPAN_OTA_STARTED,
    HOMESPAN_PAIRING_NEEDED,
    HOMESPAN_PAIRED,
    HOMESPAN_READY,
};


///////////////////////////////

// Forward-Declarations

struct Span;
struct SpanAccessory;
struct SpanService;
struct SpanCharacteristic;
struct SpanRange;
struct SpanBuf;
struct SpanButton;
struct SpanUserCommand;

extern Span homeSpan;

///////////////////////////////

struct SpanConfig {                         
  int configNumber=0;                         // configuration number - broadcast as Bonjour "c#" (computed automatically)
  uint8_t hashCode[48]={0};                   // SHA-384 hash of Span Database stored as a form of unique "signature" to know when to update the config number upon changes
};

///////////////////////////////

struct SpanBuf{                               // temporary storage buffer for use with putCharacteristicsURL() and checkTimedResets() 
  uint32_t aid=0;                             // updated aid 
  int iid=0;                                  // updated iid
  char *val=NULL;                             // updated value (optional, though either at least 'val' or 'ev' must be specified)
  char *ev=NULL;                              // updated event notification flag (optional, though either at least 'val' or 'ev' must be specified)
  StatusCode status;                          // return status (HAP Table 6-11)
  SpanCharacteristic *characteristic=NULL;    // Characteristic to update (NULL if not found)
};
  
///////////////////////////////

struct Span{

  const char *displayName;                      // display name for this device - broadcast as part of Bonjour MDNS
  const char *hostNameBase;                     // base of hostName of this device - full host name broadcast by Bonjour MDNS will have 6-byte accessoryID as well as '.local' automatically appended
  const char *hostNameSuffix=NULL;              // optional "suffix" of hostName of this device.  If specified, will be used as the hostName suffix instead of the 6-byte accessoryID
  char *hostName;                               // full host name of this device - constructed from hostNameBase and 6-byte AccessoryID
  const char *modelName;                        // model name of this device - broadcast as Bonjour field "md" 
  char category[3]="";                          // category ID of primary accessory - broadcast as Bonjour field "ci" (HAP Section 13)
  unsigned long snapTime;                       // current time (in millis) snapped before entering Service loops() or updates()
  boolean isInitialized=false;                  // flag indicating HomeSpan has been initialized
  int nFatalErrors=0;                           // number of fatal errors in user-defined configuration
  int nWarnings=0;                              // number of warnings errors in user-defined configuration
  String configLog;                             // log of configuration process, including any errors
  boolean isBridge=true;                        // flag indicating whether device is configured as a bridge (i.e. first Accessory contains nothing but AccessoryInformation and HAPProtocolInformation)
  HapQR qrCode;                                 // optional QR Code to use for pairing
  const char *sketchVersion="n/a";              // version of the sketch
  nvs_handle charNVS;                           // handle for non-volatile-storage of Characteristics data
  nvs_handle wifiNVS=0;                         // handle for non-volatile-storage of WiFi data

  boolean connected=false;                      // WiFi connection status
  unsigned long waitTime=60000;                 // time to wait (in milliseconds) between WiFi connection attempts
  unsigned long alarmConnect=0;                 // time after which WiFi connection attempt should be tried again
  
  const char *defaultSetupCode=DEFAULT_SETUP_CODE;            // Setup Code used for pairing
  uint8_t statusPin=DEFAULT_STATUS_PIN;                       // pin for status LED    
  uint8_t controlPin=DEFAULT_CONTROL_PIN;                     // pin for Control Pushbutton
  uint8_t logLevel=DEFAULT_LOG_LEVEL;                         // level for writing out log messages to serial monitor
  uint8_t maxConnections=DEFAULT_MAX_CONNECTIONS;             // number of simultaneous HAP connections
  unsigned long comModeLife=DEFAULT_COMMAND_TIMEOUT*1000;     // length of time (in milliseconds) to keep Command Mode alive before resuming normal operations
  uint16_t tcpPortNum=DEFAULT_TCP_PORT;                       // port for TCP communications between HomeKit and HomeSpan
  char qrID[5]="";                                            // Setup ID used for pairing with QR Code
  boolean otaEnabled=false;                                   // enables Over-the-Air ("OTA") updates
  char otaPwd[33];                                            // MD5 Hash of OTA password, represented as a string of hexidecimal characters
  boolean otaAuth;                                            // OTA requires password when set to true
  void (*wifiCallback)()=NULL;                                // optional callback function to invoke once WiFi connectivity is established
  boolean autoStartAPEnabled=false;                           // enables auto start-up of Access Point when WiFi Credentials not found
  void (*apFunction)()=NULL;                                  // optional function to invoke when starting Access Point
  
  WiFiServer *hapServer;                            // pointer to the HAP Server connection
  Blinker statusLED;                                // indicates HomeSpan status
  PushButton controlButton;                         // controls HomeSpan configuration and resets
  Network network;                                  // configures WiFi and Setup Code via either serial monitor or temporary Access Point
    
  SpanConfig hapConfig;                             // track configuration changes to the HAP Accessory database; used to increment the configuration number (c#) when changes found
  vector<SpanAccessory *> Accessories;              // vector of pointers to all Accessories
  vector<SpanService *> Loops;                      // vector of pointer to all Services that have over-ridden loop() methods
  vector<SpanBuf> Notifications;                    // vector of SpanBuf objects that store info for Characteristics that are updated with setVal() and require a Notification Event
  vector<SpanButton *> PushButtons;                 // vector of pointer to all PushButtons
  unordered_map<uint64_t, uint32_t> TimedWrites;    // map of timed-write PIDs and Alarm Times (based on TTLs)
  
  unordered_map<char, SpanUserCommand *> UserCommands;           // map of pointers to all UserCommands

  esp_event_loop_handle_t eventLoopHandle = NULL;
  void (*eventCallback)(int e)=NULL;                // optional function to invoke when events occur

  void begin(Category catID=DEFAULT_CATEGORY,
             const char *displayName=DEFAULT_DISPLAY_NAME,
             const char *hostNameBase=DEFAULT_HOST_NAME,
             const char *modelName=DEFAULT_MODEL_NAME);        
             
  void poll();                                  // poll HAP Clients and process any new HAP requests
  int getFreeSlot();                            // returns free HAPClient slot number. HAPClients slot keep track of each active HAPClient connection
  void checkConnect();                          // check WiFi connection; connect if needed
  void commandMode();                           // allows user to control and reset HomeSpan settings with the control button
  void processSerialCommand(const char *c);     // process command 'c' (typically from readSerial, though can be called with any 'c')

  int sprintfAttributes(char *cBuf);            // prints Attributes JSON database into buf, unless buf=NULL; return number of characters printed, excluding null terminator, even if buf=NULL
  void prettyPrint(char *buf, int nsp=2);       // print arbitrary JSON from buf to serial monitor, formatted with indentions of 'nsp' spaces
  SpanCharacteristic *find(uint32_t aid, int iid);   // return Characteristic with matching aid and iid (else NULL if not found)
  
  int countCharacteristics(char *buf);                                    // return number of characteristic objects referenced in PUT /characteristics JSON request
  int updateCharacteristics(char *buf, SpanBuf *pObj);                    // parses PUT /characteristics JSON request 'buf into 'pObj' and updates referenced characteristics; returns 1 on success, 0 on fail
  int sprintfAttributes(SpanBuf *pObj, int nObj, char *cBuf);             // prints SpanBuf object into buf, unless buf=NULL; return number of characters printed, excluding null terminator, even if buf=NULL
  int sprintfAttributes(char **ids, int numIDs, int flags, char *cBuf);   // prints accessory.characteristic ids into buf, unless buf=NULL; return number of characters printed, excluding null terminator, even if buf=NULL

  void clearNotify(int slotNum);                                          // set ev notification flags for connection 'slotNum' to false across all characteristics 
  int sprintfNotify(SpanBuf *pObj, int nObj, char *cBuf, int conNum);     // prints notification JSON into buf based on SpanBuf objects and specified connection number

  void setControlPin(uint8_t pin){controlPin=pin;}                        // sets Control Pin
  void setStatusPin(uint8_t pin){statusPin=pin;}                          // sets Status Pin
  int getStatusPin(){return(statusPin);}                                  // gets Status Pin
  void setApSSID(const char *ssid){network.apSSID=ssid;}                  // sets Access Point SSID
  void setApPassword(const char *pwd){network.apPassword=pwd;}            // sets Access Point Password
  void setApTimeout(uint16_t nSec){network.lifetime=nSec*1000;}           // sets Access Point Timeout (seconds)
  void setCommandTimeout(uint16_t nSec){comModeLife=nSec*1000;}           // sets Command Mode Timeout (seconds)
  void setLogLevel(uint8_t level){logLevel=level;}                        // sets Log Level for log messages (0=baseline, 1=intermediate, 2=all)
  void setMaxConnections(uint8_t nCon){maxConnections=nCon;}              // sets maximum number of simultaneous HAP connections (HAP requires devices support at least 8)
  void setHostNameSuffix(const char *suffix){hostNameSuffix=suffix;}      // sets the hostName suffix to be used instead of the 6-byte AccessoryID
  void setPortNum(uint16_t port){tcpPortNum=port;}                        // sets the TCP port number to use for communications between HomeKit and HomeSpan
  void setQRID(const char *id);                                           // sets the Setup ID for optional pairing with a QR Code
  void enableOTA(boolean auth=true){otaEnabled=true;otaAuth=auth;}        // enables Over-the-Air updates, with (auth=true) or without (auth=false) authorization password
  void setSketchVersion(const char *sVer){sketchVersion=sVer;}            // set optional sketch version number
  const char *getSketchVersion(){return sketchVersion;}                   // get sketch version number
  void setWifiCallback(void (*f)()){wifiCallback=f;}                      // sets an optional user-defined function to call once WiFi connectivity is established
  void setApFunction(void (*f)()){apFunction=f;}                          // sets an optional user-defined function to call when activating the WiFi Access Point
  
  void enableAutoStartAP(){autoStartAPEnabled=true;}                      // enables auto start-up of Access Point when WiFi Credentials not found
  void setWifiCredentials(const char *ssid, const char *pwd);             // sets WiFi Credentials

  void addEventCallback(void (*f)(int e));                                // adds an event callback
  void fireEventCallback(int e);                                          // fires an event
};

///////////////////////////////

struct SpanAccessory{
    
  uint32_t aid=0;                           // Accessory Instance ID (HAP Table 6-1)
  int iidCount=0;                           // running count of iid to use for Services and Characteristics associated with this Accessory                                 
  vector<SpanService *> Services;           // vector of pointers to all Services in this Accessory  

  SpanAccessory(uint32_t aid=0);

  int sprintfAttributes(char *cBuf);        // prints Accessory JSON database into buf, unless buf=NULL; return number of characters printed, excluding null terminator, even if buf=NULL  
  void validate();                          // error-checks Accessory
};

///////////////////////////////

struct SpanService{

  int iid=0;                                              // Instance ID (HAP Table 6-2)
  const char *type;                                       // Service Type
  const char *hapName;                                    // HAP Name
  boolean hidden=false;                                   // optional property indicating service is hidden
  boolean primary=false;                                  // optional property indicating service is primary
  vector<SpanCharacteristic *> Characteristics;           // vector of pointers to all Characteristics in this Service  
  vector<HapChar *> req;                                  // vector of pointers to all required HAP Characteristic Types for this Service
  vector<HapChar *> opt;                                  // vector of pointers to all optional HAP Characteristic Types for this Service
  vector<SpanService *> linkedServices;                   // vector of pointers to any optional linked Services
  
  SpanService(const char *type, const char *hapName);

  SpanService *setPrimary();                              // sets the Service Type to be primary and returns pointer to self
  SpanService *setHidden();                               // sets the Service Type to be hidden and returns pointer to self
  SpanService *addLink(SpanService *svc);                 // adds svc as a Linked Service and returns pointer to self

  int sprintfAttributes(char *cBuf);                      // prints Service JSON records into buf; return number of characters printed, excluding null terminator
  void validate();                                        // error-checks Service
  
  virtual boolean update() {return(true);}                // placeholder for code that is called when a Service is updated via a Controller.  Must return true/false depending on success of update
  virtual void loop(){}                                   // loops for each Service - called every cycle and can be over-ridden with user-defined code
  virtual void button(int pin, int pressType){}           // method called for a Service when a button attached to "pin" has a Single, Double, or Long Press, according to pressType
};

///////////////////////////////

struct SpanCharacteristic{


  union UVal {                                  
    boolean BOOL;
    uint8_t UINT8;
    uint16_t UINT16;
    uint32_t UINT32;
    uint64_t UINT64;
    int32_t INT;
    double FLOAT;
    char *STRING = NULL;
  };

  int iid=0;                               // Instance ID (HAP Table 6-3)
  const char *type;                        // Characteristic Type
  const char *hapName;                     // HAP Name
  UVal value;                              // Characteristic Value
  uint8_t perms;                           // Characteristic Permissions
  FORMAT format;                           // Characteristic Format        
  char *desc=NULL;                         // Characteristic Description (optional)
  UVal minValue;                           // Characteristic minimum (not applicable for STRING)
  UVal maxValue;                           // Characteristic maximum (not applicable for STRING)
  UVal stepValue;                          // Characteristic step size (not applicable for STRING)
  boolean staticRange;                     // Flag that indiates whether Range is static and cannot be changed with setRange()
  boolean customRange=false;               // Flag for custom ranges
  boolean *ev;                             // Characteristic Event Notify Enable (per-connection)
  char *nvsKey=NULL;                       // key for NVS storage of Characteristic value
  
  uint32_t aid=0;                          // Accessory ID - passed through from Service containing this Characteristic
  boolean isUpdated=false;                 // set to true when new value has been requested by PUT /characteristic
  unsigned long updateTime=0;              // last time value was updated (in millis) either by PUT /characteristic OR by setVal()
  UVal newValue;                           // the updated value requested by PUT /characteristic
  SpanService *service=NULL;               // pointer to Service containing this Characteristic
      
  SpanCharacteristic(HapChar *hapChar);           // contructor
  
  int sprintfAttributes(char *cBuf, int flags);   // prints Characteristic JSON records into buf, according to flags mask; return number of characters printed, excluding null terminator  
  StatusCode loadUpdate(char *val, char *ev);     // load updated val/ev from PUT /characteristic JSON request.  Return intiial HAP status code (checks to see if characteristic is found, is writable, etc.)
  
  boolean updated(){return(isUpdated);}           // returns isUpdated
  unsigned long timeVal();                        // returns time elapsed (in millis) since value was last updated
    
  String uvPrint(UVal &u){
    char c[64];
    switch(format){
      case FORMAT::BOOL:
        return(String(u.BOOL));      
      case FORMAT::INT:
        return(String(u.INT));
      case FORMAT::UINT8:
        return(String(u.UINT8));        
      case FORMAT::UINT16:
        return(String(u.UINT16));        
      case FORMAT::UINT32:
        return(String(u.UINT32));        
      case FORMAT::UINT64:
        sprintf(c,"%llu",u.UINT64);
        return(String(c));        
      case FORMAT::FLOAT:
        sprintf(c,"%llg",u.FLOAT);
        return(String(c));        
      case FORMAT::STRING:
        sprintf(c,"\"%s\"",u.STRING);
        return(String(c));        
    } // switch
  } // str()

  void uvSet(UVal &u, const char *val){
    u.STRING = (char *)realloc(u.STRING, strlen(val) + 1);
    strncpy(u.STRING, val, strlen(val));
    u.STRING[strlen(val)] = '\0';
  }

  char *getString(){
    if(format == FORMAT::STRING)
        return value.STRING;

    return NULL;
  }

  char *getNewString(){
    if(format == FORMAT::STRING)
        return newValue.STRING;

    return NULL;
  }

  template <typename T> void uvSet(UVal &u, T val){  
    switch(format){
      case FORMAT::BOOL:
        u.BOOL=(boolean)val;
      break;
      case FORMAT::INT:
        u.INT=(int)val;
      break;
      case FORMAT::UINT8:
        u.UINT8=(uint8_t)val;
      break;
      case FORMAT::UINT16:
        u.UINT16=(uint16_t)val;
      break;
      case FORMAT::UINT32:
        u.UINT32=(uint32_t)val;
      break;
      case FORMAT::UINT64:
        u.UINT64=(uint64_t)val;
      break;
      case FORMAT::FLOAT:
        u.FLOAT=(double)val;
      break;
    } // switch
  } // set()
 
  template <class T> T uvGet(UVal &u){
  
    switch(format){   
      case FORMAT::BOOL:
        return((T) u.BOOL);        
      case FORMAT::INT:
        return((T) u.INT);        
      case FORMAT::UINT8:
        return((T) u.UINT8);        
      case FORMAT::UINT16:
        return((T) u.UINT16);        
      case FORMAT::UINT32:
        return((T) u.UINT32);        
      case FORMAT::UINT64:
        return((T) u.UINT64);        
      case FORMAT::FLOAT:
        return((T) u.FLOAT);        
      case FORMAT::STRING:
        Serial.print("\n*** WARNING:  Can't use getVal() or getNewVal() with string Characteristics.\n\n");
        return(0);
    }
  } // get()
    
  template <typename A, typename B, typename S=int> SpanCharacteristic *setRange(A min, B max, S step=0){

    char c[256];
    homeSpan.configLog+=String("         \u2b0c Set Range for ") + String(hapName) + " with IID=" + String(iid);

    if(customRange){
      sprintf(c,"  *** ERROR!  Range already set for this Characteristic! ***\n");
      homeSpan.nFatalErrors++;
    } else 
        
    if(staticRange){     
      sprintf(c,"  *** ERROR!  Can't change range for this Characteristic! ***\n");
      homeSpan.nFatalErrors++;
    } else {
      
      uvSet(minValue,min);
      uvSet(maxValue,max);
      uvSet(stepValue,step);  
      customRange=true; 
      
      if(uvGet<double>(stepValue)>0)
        sprintf(c,": Min=%s, Max=%s, Step=%s\n",uvPrint(minValue),uvPrint(maxValue),uvPrint(stepValue));
      else
        sprintf(c,": Min=%s, Max=%s\n",uvPrint(minValue),uvPrint(maxValue));        
    }
    homeSpan.configLog+=c;         
    return(this);
    
  } // setRange()
    
  template <typename T, typename A=boolean, typename B=boolean> void init(T val, boolean nvsStore, A min=0, B max=1){

    int nvsFlag=0;

    uvSet(value,val);
    uvSet(newValue,val);

    if(format != FORMAT::STRING) {
        uvSet(minValue,min);
        uvSet(maxValue,max);
        uvSet(stepValue,0);
    }

    if(nvsStore){
      nvsKey=(char *)malloc(16);
      uint16_t t;
      sscanf(type,"%x",&t);
      sprintf(nvsKey,"%04X%08X%03X",t,aid,iid&0xFFF);
      size_t len;
    
      if(!nvs_get_blob(homeSpan.charNVS,nvsKey,NULL,&len)){
        nvs_get_blob(homeSpan.charNVS,nvsKey,&value,&len);
        newValue=value;
        nvsFlag=2;
      }
      else {
        nvs_set_blob(homeSpan.charNVS,nvsKey,&value,sizeof(UVal));       // store data
        nvs_commit(homeSpan.charNVS);                                    // commit to NVS  
        nvsFlag=1;
      }
    }
  
    homeSpan.configLog+="(" + uvPrint(value) + ")" + ":  IID=" + String(iid) + ", UUID=0x" + String(type);
    if(format!=FORMAT::STRING && format!=FORMAT::BOOL)
      homeSpan.configLog+= "  Range=[" + String(uvPrint(minValue)) + "," + String(uvPrint(maxValue)) + "]";

    if(nvsFlag==2)
      homeSpan.configLog+=" (restored)";
    else if(nvsFlag==1)
      homeSpan.configLog+=" (storing)";
  
    boolean valid=false;
  
    for(int i=0; !valid && i<homeSpan.Accessories.back()->Services.back()->req.size(); i++)
      valid=!strcmp(type,homeSpan.Accessories.back()->Services.back()->req[i]->type);
  
    for(int i=0; !valid && i<homeSpan.Accessories.back()->Services.back()->opt.size(); i++)
      valid=!strcmp(type,homeSpan.Accessories.back()->Services.back()->opt[i]->type);
  
    if(!valid){
      homeSpan.configLog+=" *** ERROR!  Service does not support this Characteristic. ***";
      homeSpan.nFatalErrors++;
    }
  
    boolean repeated=false;
    
    for(int i=0; !repeated && i<homeSpan.Accessories.back()->Services.back()->Characteristics.size(); i++)
      repeated=!strcmp(type,homeSpan.Accessories.back()->Services.back()->Characteristics[i]->type);
    
    if(valid && repeated){
      homeSpan.configLog+=" *** ERROR!  Characteristic already defined for this Service. ***";
      homeSpan.nFatalErrors++;
    }
  
    homeSpan.Accessories.back()->Services.back()->Characteristics.push_back(this);  
  
    homeSpan.configLog+="\n"; 
   
  } // init()

  template <class T=int> T getVal(){
    return(uvGet<T>(value));
  }

  template <class T=int> T getNewVal(){
    return(uvGet<T>(newValue));
  }
    
  template <typename T> void setVal(T val){

    if(format==FORMAT::STRING && perms & PW == 0){
      Serial.printf("\n*** WARNING:  Attempt to update Characteristic::%s(\"%s\") with setVal() ignored.  No WRITE permission on this characteristic\n\n",hapName,value.STRING);
      return;
    }

    if(format!=FORMAT::STRING && ( val < uvGet<T>(minValue) || val > uvGet<T>(maxValue))){
      Serial.printf("\n*** WARNING:  Attempt to update Characteristic::%s with setVal(%llg) is out of range [%llg,%llg].  This may cause device to become non-reponsive!\n\n",
      hapName,(double)val,uvGet<double>(minValue),uvGet<double>(maxValue));
    }
   
    uvSet(value,val);
    uvSet(newValue,val);
      
    updateTime=homeSpan.snapTime;
    
    SpanBuf sb;                             // create SpanBuf object
    sb.characteristic=this;                 // set characteristic          
    sb.status=StatusCode::OK;               // set status
    char dummy[]="";
    sb.val=dummy;                           // set dummy "val" so that sprintfNotify knows to consider this "update"
    homeSpan.Notifications.push_back(sb);   // store SpanBuf in Notifications vector  

    if(nvsKey){
      nvs_set_blob(homeSpan.charNVS,nvsKey,&value,sizeof(UVal));    // store data
      nvs_commit(homeSpan.charNVS);
    }
    
  } // setVal()
  
};

///////////////////////////////

struct SpanRange{
  SpanRange(int min, int max, int step);
};

///////////////////////////////

struct SpanButton{

  enum {
    SINGLE=0,
    DOUBLE=1,
    LONG=2
  };
  
  int pin;                       // pin number  
  uint16_t singleTime;           // minimum time (in millis) required to register a single press
  uint16_t longTime;             // minimum time (in millis) required to register a long press
  uint16_t doubleTime;           // maximum time (in millis) between single presses to register a double press instead
  SpanService *service;          // Service to which this PushButton is attached

  PushButton *pushButton;        // PushButton associated with this SpanButton

  SpanButton(int pin, uint16_t longTime=2000, uint16_t singleTime=5, uint16_t doubleTime=200);
};

///////////////////////////////

struct SpanUserCommand {
  const char *s;                              // description of command
  void (*userFunction)(const char *v);              // user-defined function to call

  SpanUserCommand(char c, const char *s, void (*f)(const char *v));  
};

/////////////////////////////////////////////////

#include "Span.h"
