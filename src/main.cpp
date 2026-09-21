#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <time.h>
#include <SD.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

TFT_eSPI tft(240, 320);
SPIClass touchSPI = SPIClass(VSPI);
SPIClass sdSPI = SPIClass(HSPI);
XPT2046_Touchscreen touch(33, 36);
Preferences prefs;
Preferences netPrefs;
WebServer webServer(80);
bool sdArtReady = false;
bool portalActive = false;
bool webRoutesReady = false;
String savedWifiSsid;
String savedWifiPassword;
String savedTimezone="PST8PDT,M3.2.0,M11.1.0";
String savedZipcode;
float skyLatitude=0.0f;
float skyLongitude=0.0f;
bool skyLocationSet=false;
bool clockSynchronized=false;
bool internetReady=false;
uint32_t lastClockRequest=0;
uint32_t lastCosmicRefresh=0;
const char* COSMIC_FEED_URL="https://raw.githubusercontent.com/vrocco/astral-cabinet/main/data/cosmic.txt";

constexpr int W=320, H=240;
constexpr int TOUCH_X_MIN=200, TOUCH_X_MAX=3900, TOUCH_Y_MIN=200, TOUCH_Y_MAX=3900;
const uint16_t INK=0x0841, NAVY=0x10A3, BURG=0x780C, GOLD=0xD5A5, CREAM=0xFFDB, MUTED=0xB5B6, BLACK=0x0000;
const char* DEFAULT_NAME="Astral Guest";
uint8_t displayProfile=0, previewDisplayProfile=0;
struct TimezoneChoice { const char* value; const char* label; };
const TimezoneChoice timezones[] = {
 {"PST8PDT,M3.2.0,M11.1.0","Pacific (Los Angeles)"},
 {"MST7MDT,M3.2.0,M11.1.0","Mountain (Denver)"},
 {"CST6CDT,M3.2.0,M11.1.0","Central (Chicago)"},
 {"EST5EDT,M3.2.0,M11.1.0","Eastern (New York)"},
 {"UTC0","UTC"}
};

struct Zodiac { const char* name; const char* glyph; const char* element; const char* theme; };
const Zodiac signs[] = {
 {"Aries","RAM","Fire","begin boldly"},{"Taurus","BULL","Earth","trust what endures"},{"Gemini","TWINS","Air","follow curiosity"},{"Cancer","CRAB","Water","protect what matters"},
 {"Leo","LION","Fire","let yourself shine"},{"Virgo","MAIDEN","Earth","make the small thing well"},{"Libra","SCALES","Air","seek the truest balance"},{"Scorpio","SCORPION","Water","look beneath the surface"},
 {"Sagittarius","ARCHER","Fire","aim beyond the familiar"},{"Capricorn","GOAT","Earth","climb one sure step"},{"Aquarius","WATER BEARER","Air","question the expected"},{"Pisces","FISH","Water","listen to intuition"}
};
struct Card { const char* name; const char* key; const char* up; const char* rev; };
const Card cards[] = {
 {"The Fool","beginnings","A threshold opens. Bring courage, curiosity, and only what you truly need.","A new beginning may need one more moment of preparation. Notice the edge before you leap."},
 {"The Magician","agency","The tools are already in your hands. Make one clear choice and put it into motion.","Your energy is scattered. Choose one tool, one promise, and one honest next action."},
 {"The High Priestess","intuition","The quiet answer is still an answer. Give your inner knowing room to speak.","Too much outside noise is clouding the signal. Pause before asking everyone else what to do."},
 {"The Empress","growth","Nourish what you want to grow. Beauty, patience, and generosity are active forces.","You may be giving beyond your reserves. Restore yourself before tending every other garden."},
 {"The Emperor","structure","A boundary or plan will make freedom possible. Lead with steadiness, not force.","Control is becoming heavier than the problem. Leave room for another person’s wisdom."},
 {"The Hierophant","tradition","A trusted teaching or mentor can illuminate the next step without choosing it for you.","A rule may no longer fit the life you are actually living. Examine the source of the advice."},
 {"The Lovers","alignment","A meaningful choice asks you to act according to your values, not merely your fears.","Mixed signals point to an unspoken value. Say what matters before deciding."},
 {"The Chariot","momentum","Two competing forces can move together when your direction is clear. Keep your hands on the reins.","Momentum without direction exhausts. Revisit the destination before pressing forward."},
 {"Strength","heart","Gentleness is not surrender. Patient courage will accomplish more than a clenched fist.","Treat yourself with the compassion you are offering everyone else."},
 {"The Hermit","reflection","Step away from the noise long enough to hear your own answer. Solitude can clarify, not isolate.","Reflection has become hiding. Let one trusted person see where you are."},
 {"Wheel of Fortune","change","A cycle is turning. Meet changing circumstances with flexibility and attention.","Do not mistake a temporary turn for a permanent fate. The wheel keeps moving."},
 {"Justice","truth","Name the facts plainly. A fair decision begins when wishful thinking ends.","An imbalance wants acknowledgment. Repair what you can without rewriting the truth."},
 {"The Hanged One","perspective","Release the usual angle. A pause may reveal that the real answer is a different question.","You are waiting for certainty that only action can create. Change your vantage point."},
 {"Death","release","An old form is ending so something more honest can begin. Let the unnecessary fall away.","Resistance is prolonging a transition. Ask what you are keeping only from habit."},
 {"Temperance","harmony","Blend the extremes. Progress arrives through a sustainable rhythm rather than a dramatic push.","Your elements are out of balance. Make one small adjustment before making a large promise."},
 {"The Devil","attachment","Look closely at the bargain. Naming the chain is the first act of freedom.","A fear has been given more authority than it deserves. Reclaim one choice today."},
 {"The Tower","awakening","A false certainty may crack. Build again from what remains real, not what merely looked stable.","Avoiding a necessary truth makes the eventual change louder. Open the window yourself."},
 {"The Star","hope","Keep moving toward what restores your hope. A small light is still a direction.","Hope is present but needs care. Protect the practice that reconnects you to it."},
 {"The Moon","mystery","Not every shadow is a warning. Move slowly, check assumptions, and let clarity arrive in stages.","Anxiety is filling gaps with stories. Return to what you can actually observe."},
 {"The Sun","vitality","Let the good news be good. Visibility, warmth, and honest joy are available now.","Do not postpone joy until everything is perfect. Let a little light in through the unfinished parts."},
 {"Judgement","calling","A past lesson is ready to become present wisdom. Answer the invitation to renew your direction.","You are judging an earlier version of yourself too harshly. Learn from it, then continue."},
 {"The World","completion","A cycle has gathered its meaning. Honor how far you have come before opening the next door.","Something is almost complete, but one loose thread deserves your attention."}
};

enum Screen { BOOT, JOURNEY, ONLINE_SETUP, HOME, SETTINGS, DISPLAY_CALIBRATION, CONFIRM_NETWORK_DELETE, ZODIAC, MENU, DAILY, SPREAD, CARD, CABINET, ELEMENTAL_RITUAL, LIVE_ASTRAL, SKY_NOW, RITUAL_CALENDAR, COSMIC_WEATHER };
Screen screen=BOOT; int signIndex=0, spreadMode=0, reveal=0, detailCard=0, ritualVariant=0, ritualStep=0; int drawn[3]={0,0,0}; bool reversed[3]={false,false,false}; String guestName;
uint32_t uiRevision=0;
void textWrap(const String&s,int x,int y,int size,uint16_t color,int width,int maxLines=0);
String readSdText(const char* path, size_t limit=8192);

// The CYD's microSD socket is on its own HSPI bus: SCK=18, MISO=19,
// MOSI=23, CS=5. Keeping it separate from the TFT and touch buses prevents
// asset decoding from disturbing the calibrated touch controller.
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap){
  if(y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}
String artPath(uint8_t id, bool thumbnail=false, bool reversed=false){
  char path[28];
  const char* suffix=reversed ? (thumbnail ? "_rs" : "_r") : (thumbnail ? "_s" : "");
  snprintf(path, sizeof(path), "/astral/%02u%s.jpg", id, suffix);
  return String(path);
}
bool drawSdArt(const String& path, int x, int y){
  if(!sdArtReady) return false;
  if(!SD.exists(path)){
    Serial.printf("ASTRAL: art missing %s\n", path.c_str());
    return false;
  }
  JRESULT result=TJpgDec.drawSdJpg(x, y, path.c_str());
  if(result != JDR_OK) Serial.printf("ASTRAL: art decode failed %s result=%d\n", path.c_str(), result);
  return result == JDR_OK;
}
bool drawSdCard(uint8_t id, int x, int y, bool thumbnail=false, bool reversed=false){ return drawSdArt(artPath(id, thumbnail, reversed), x, y); }
bool drawSdCardBack(int x,int y,bool thumbnail=false){ return drawSdArt(thumbnail ? "/astral/card_back_s.jpg" : "/astral/card_back.jpg", x, y); }
String zodiacArtPath(uint8_t id){ char path[32]; snprintf(path, sizeof(path), "/astral/zodiac/%02u.jpg", id); return String(path); }
bool drawZodiacArt(uint8_t id, int x, int y){ return drawSdArt(zodiacArtPath(id), x, y); }

String htmlEscape(const String& value){
  String escaped;
  for(unsigned i=0;i<value.length();i++){
    switch(value[i]){
      case '&': escaped += F("&amp;"); break;
      case '<': escaped += F("&lt;"); break;
      case '>': escaped += F("&gt;"); break;
      case '"': escaped += F("&quot;"); break;
      case '\'': escaped += F("&#39;"); break;
      default: escaped += value[i]; break;
    }
  }
  return escaped;
}

String nearbyNetworkOptions(){
  String options=F("<option value=\"\">Choose a network</option>");
  int count=WiFi.scanNetworks();
  for(int i=0;i<count && i<20;i++){
    String ssid=WiFi.SSID(i);
    if(!ssid.length()) continue;
    String safe=htmlEscape(ssid);
    options += F("<option value=\""); options += safe; options += F("\">");
    options += safe; options += F(" ("); options += String(WiFi.RSSI(i)); options += F(" dBm)</option>");
  }
  WiFi.scanDelete();
  return options;
}

String readSdText(const char* path, size_t limit){
  if(!sdArtReady) return String();
  File file=SD.open(path,FILE_READ);
  if(!file) return String();
  String text; text.reserve(file.size());
  while(file.available() && text.length()<limit) text += char(file.read());
  file.close();
  return text;
}

String timezoneOptions(){
  String options;
  for(const auto& zone:timezones){
    options += F("<option value=\""); options += zone.value; options += F("\"");
    if(savedTimezone==zone.value) options += F(" selected");
    options += F(">"); options += zone.label; options += F("</option>");
  }
  return options;
}

String setupPortalPage(){
  String page=readSdText("/astral/setup.html");
  if(!page.length()) page=F("<!doctype html><html><body><h1>Maxine's Astral Cabinet</h1><form method=\"post\" action=\"/save\"><label>Network <select name=\"ssid\">{{NETWORK_OPTIONS}}</select></label><label>Manual network <input name=\"manual\"></label><label>Password <input name=\"password\" type=\"password\"></label><button>Save and restart</button></form></body></html>");
  page.replace("{{NETWORK_OPTIONS}}",nearbyNetworkOptions());
  page.replace("{{TIMEZONE_OPTIONS}}",timezoneOptions());
  page.replace("{{ZIPCODE}}",htmlEscape(savedZipcode));
  page.replace("{{LATITUDE}}",skyLocationSet?String(skyLatitude,4):String());
  page.replace("{{LONGITUDE}}",skyLocationSet?String(skyLongitude,4):String());
  return page;
}

void handleSetupPortal(){ webServer.send(200,"text/html",setupPortalPage()); }

void handleSaveNetwork(){
  String ssid=webServer.arg("manual");
  if(!ssid.length()) ssid=webServer.arg("ssid");
  String password=webServer.arg("password");
  String timezone=webServer.arg("timezone");
  String zipcode=webServer.arg("zipcode"); zipcode.trim();
  bool timezoneKnown=!timezone.length();
  for(const auto& zone:timezones) if(timezone==zone.value) timezoneKnown=true;
  String latitudeArg=webServer.arg("latitude"), longitudeArg=webServer.arg("longitude");
  bool hasPhoneLocation=latitudeArg.length() && longitudeArg.length();
  float latitude=hasPhoneLocation?latitudeArg.toFloat():0.0f;
  float longitude=hasPhoneLocation?longitudeArg.toFloat():0.0f;
  if(!ssid.length() || ssid.length()>32 || password.length()>63 || zipcode.length()>10 || !timezoneKnown || (latitudeArg.length()!=longitudeArg.length()) || (hasPhoneLocation && (latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180))){
    webServer.send(400,"text/html",F("<html><body><h2>Network details are incomplete.</h2><p><a href=\"/\">Return to setup</a></p></body></html>"));
    return;
  }
  if(timezone.length()) savedTimezone=timezone;
  savedZipcode=zipcode;
  skyLocationSet=hasPhoneLocation;
  if(skyLocationSet){ skyLatitude=latitude; skyLongitude=longitude; }
  netPrefs.putString("ssid",ssid);
  netPrefs.putString("password",password);
  netPrefs.putString("timezone",savedTimezone);
  netPrefs.putString("zipcode",savedZipcode);
  netPrefs.putBool("locationSet",skyLocationSet);
  if(skyLocationSet){ netPrefs.putFloat("latitude",skyLatitude); netPrefs.putFloat("longitude",skyLongitude); }
  else { netPrefs.remove("latitude"); netPrefs.remove("longitude"); }
  webServer.send(200,"text/html",F("<html><body><h2>Saved.</h2><p>Maxine's Astral Cabinet is restarting to join the selected network.</p></body></html>"));
  delay(750);
  ESP.restart();
}

void configureWebRoutes(){
  if(webRoutesReady) return;
  webServer.on("/",HTTP_GET,handleSetupPortal);
  webServer.on("/save",HTTP_POST,handleSaveNetwork);
  webServer.onNotFound([](){ webServer.sendHeader("Location","/"); webServer.send(302,"text/plain",""); });
  webRoutesReady=true;
}

void startConfigPortal(){
  if(portalActive) return;
  WiFi.mode(WIFI_AP_STA);
  const IPAddress apIp(192,168,4,1), gateway(192,168,4,1), subnet(255,255,255,0);
  WiFi.softAPConfig(apIp,gateway,subnet);
  WiFi.softAP("astral");
  configureWebRoutes();
  webServer.begin();
  portalActive=true;
  Serial.println("ASTRAL: Wi-Fi setup at SSID astral / http://192.168.4.1");
}

void stopConfigPortal(){
  if(!portalActive) return;
  webServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  if(savedWifiSsid.length()) WiFi.begin(savedWifiSsid.c_str(),savedWifiPassword.c_str());
  portalActive=false;
}

void connectSavedWifi(){
  savedWifiSsid=netPrefs.getString("ssid","");
  savedWifiPassword=netPrefs.getString("password","");
  savedTimezone=netPrefs.getString("timezone",savedTimezone);
  savedZipcode=netPrefs.getString("zipcode","");
  skyLocationSet=netPrefs.getBool("locationSet",false);
  if(skyLocationSet){ skyLatitude=netPrefs.getFloat("latitude",0.0f); skyLongitude=netPrefs.getFloat("longitude",0.0f); }
  if(!savedWifiSsid.length()) return;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(savedWifiSsid.c_str(),savedWifiPassword.c_str());
  Serial.printf("ASTRAL: joining saved Wi-Fi %s\n",savedWifiSsid.c_str());
}

bool internetAvailable(uint32_t waitMs=0){
  uint32_t started=millis();
  do {
    if(WiFi.status()==WL_CONNECTED){
      WiFiClient client;
      client.setTimeout(600);
      if(client.connect(IPAddress(1,1,1,1),443)){ client.stop(); return true; }
    }
    if(!waitMs || millis()-started>=waitMs) break;
    delay(100);
  } while(true);
  return false;
}

void requestClockSync(){
  if(WiFi.status()!=WL_CONNECTED || (lastClockRequest && millis()-lastClockRequest<10000)) return;
  lastClockRequest=millis();
  configTzTime(savedTimezone.c_str(),"pool.ntp.org","time.nist.gov");
  Serial.println("ASTRAL: NTP synchronization requested");
}

bool clockIsValid(){ return time(nullptr)>1700000000; }

void maintainClock(){
  if(!clockIsValid()) requestClockSync();
  clockSynchronized=clockIsValid();
}

struct LunarInfo { float age; int illumination; const char* phase; float nextNew; float nextFull; };
LunarInfo lunarInfo(){
  if(!clockIsValid()) return {0,0,"the moon keeps its own time",0,0};
  constexpr float synodic=29.530588853f;
  float age=fmodf((float(time(nullptr))-947182440.0f)/86400.0f,synodic);
  if(age<0) age+=synodic;
  float illumination=(1.0f-cosf(2.0f*PI*age/synodic))*50.0f;
  const char* phase=age<1.84566f?"new moon":age<5.53699f?"waxing crescent":age<9.22831f?"first quarter":age<12.91963f?"waxing gibbous":age<16.61096f?"full moon":age<20.30228f?"waning gibbous":age<23.99361f?"last quarter":"waning crescent";
  float nextNew=synodic-age;
  float nextFull=age<synodic/2.0f ? synodic/2.0f-age : synodic*1.5f-age;
  return {age,int(illumination+0.5f),phase,nextNew,nextFull};
}

String lunarCountdown(float days){
  int hours=int(days*24.0f+0.5f);
  return String(hours/24)+"d "+String(hours%24)+"h";
}

String localDateTime(){
  if(!clockIsValid()) return "Awaiting a time signal";
  struct tm local; time_t now=time(nullptr); localtime_r(&now,&local);
  char formatted[36]; strftime(formatted,sizeof(formatted),"%a, %b %d · %I:%M %p",&local);
  return String(formatted);
}

bool refreshCosmicCache(){
  if(!internetReady || !sdArtReady || (lastCosmicRefresh && millis()-lastCosmicRefresh<21600000UL)) return false;
  lastCosmicRefresh=millis();
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if(!http.begin(client,COSMIC_FEED_URL)) return false;
  http.setTimeout(5000);
  int status=http.GET();
  int contentLength=http.getSize();
  if(status!=HTTP_CODE_OK || contentLength<24 || contentLength>1600){ http.end(); return false; }
  SD.remove("/astral/cosmic.tmp");
  File cache=SD.open("/astral/cosmic.tmp",FILE_WRITE);
  if(!cache){ http.end(); return false; }
  int written=http.writeToStream(&cache);
  cache.close(); http.end();
  if(written!=contentLength){ SD.remove("/astral/cosmic.tmp"); return false; }
  String candidate=readSdText("/astral/cosmic.tmp",1600);
  if(!candidate.startsWith("ASTRAL COSMIC WEATHER")){ SD.remove("/astral/cosmic.tmp"); return false; }
  SD.remove("/astral/cosmic.txt");
  return SD.rename("/astral/cosmic.tmp","/astral/cosmic.txt");
}

String cosmicLine(uint8_t wanted){
  String cache=readSdText("/astral/cosmic.txt",1600);
  int line=0, start=0;
  for(unsigned i=0;i<=cache.length();i++){
    if(i==cache.length() || cache[i]=='\n'){
      if(line==wanted){ String value=cache.substring(start,i); value.trim(); return value; }
      line++; start=i+1;
    }
  }
  return String();
}

bool cosmicCacheCurrent(){
  if(!clockIsValid() || cosmicLine(0)!="ASTRAL COSMIC WEATHER") return false;
  struct tm utc; time_t now=time(nullptr); gmtime_r(&now,&utc);
  char expected[32]; strftime(expected,sizeof(expected),"Updated %Y-%m-%d UTC",&utc);
  return cosmicLine(1)==expected;
}

float cachedEventDays(uint8_t lineIndex){
  String line=cosmicLine(lineIndex);
  int split=line.indexOf(':');
  if(split<0) return -1;
  time_t event=time_t(line.substring(split+1).toInt());
  if(event<=time(nullptr)) return -1;
  return float(event-time(nullptr))/86400.0f;
}

void text(const String&s,int x,int y,int size=1,uint16_t c=CREAM){ tft.setTextWrap(false,false); tft.setTextColor(c,INK); tft.setTextSize(size); tft.setCursor(x,y); tft.print(s); }
void center(const String&s,int y,int size=1,uint16_t c=CREAM){ tft.setTextSize(size); int x=(W-tft.textWidth(s))/2; text(s,x,y,size,c); }
void panelTitle(const String&s,int x,int y,int width){ tft.setTextSize(2); text(s,x,y,tft.textWidth(s)<=width?2:1,CREAM); }
void frame(const String&title){
  tft.fillScreen(INK); tft.drawRect(5,5,W-10,H-10,GOLD); tft.drawRect(10,10,W-20,H-20,BURG);
  center(title,16,2,GOLD); tft.drawFastHLine(25,40,W-50,GOLD);
  if(screen!=HOME){
    constexpr int homeX=14, homeY=16, homeW=32, homeH=15;
    tft.fillRoundRect(homeX,homeY,homeW,homeH,4,NAVY);
    tft.drawRoundRect(homeX,homeY,homeW,homeH,4,GOLD);
    tft.setTextWrap(false,false); tft.setTextColor(CREAM,NAVY); tft.setTextSize(1);
    tft.setCursor(homeX+(homeW-tft.textWidth("BACK"))/2,homeY+(homeH-8)/2);
    tft.print("BACK");
  }
}
void button(int x,int y,int w,int h,const String&label,uint16_t fill=BURG){ tft.fillRoundRect(x,y,w,h,6,fill); tft.drawRoundRect(x,y,w,h,6,GOLD); tft.setTextWrap(false,false); tft.setTextColor(CREAM,fill); tft.setTextSize(1); int tx=x+(w-tft.textWidth(label))/2; tft.setCursor(tx,y+(h-8)/2); tft.print(label); }
void doorPlaque(int x,int y,int w,const String&label,uint16_t fill=NAVY){ tft.fillRoundRect(x,y,w,15,3,fill); tft.drawRoundRect(x,y,w,15,3,GOLD); tft.setTextWrap(false,false); tft.setTextColor(CREAM,fill); tft.setTextSize(1); tft.setCursor(x+(w-tft.textWidth(label))/2,y+4); tft.print(label); }
void footer(){ center("MAXINE'S ASTRAL CABINET",222,1,MUTED); }
void overlayCenter(const String&s,int y,int size=1,uint16_t c=CREAM){ tft.setTextWrap(false,false); tft.setTextColor(c); tft.setTextSize(size); tft.setCursor((W-tft.textWidth(s))/2,y); tft.print(s); }
void overlayAt(const String&s,int cx,int y,int size=1,uint16_t c=CREAM){ tft.setTextWrap(false,false); tft.setTextColor(c); tft.setTextSize(size); tft.setCursor(cx-tft.textWidth(s)/2,y); tft.print(s); }
struct PanelProfile { const char* name; bool standardGamma; bool rgbOrder; bool inverted; };
const PanelProfile panelProfiles[] = {
  {"LEGACY / BGR / INVERT",false,false,true}, {"STANDARD / BGR / INVERT",true,false,true},
  {"LEGACY / BGR / NORMAL",false,false,false}, {"STANDARD / BGR / NORMAL",true,false,false},
  {"LEGACY / RGB / INVERT",false,true,true}, {"STANDARD / RGB / INVERT",true,true,true},
  {"LEGACY / RGB / NORMAL",false,true,false}, {"STANDARD / RGB / NORMAL",true,true,false}
};
const uint8_t gammaStandardPositive[]={0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00};
const uint8_t gammaStandardNegative[]={0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F};
const uint8_t gammaLegacyPositive[]={0x0F,0x2A,0x28,0x08,0x0E,0x08,0x54,0xA9,0x43,0x0A,0x0F,0x00,0x00,0x00,0x00};
const uint8_t gammaLegacyNegative[]={0x00,0x15,0x17,0x07,0x11,0x06,0x2B,0x56,0x3C,0x05,0x10,0x0F,0x3F,0x3F,0x0F};
void writePanelCommand(uint8_t command,const uint8_t* data,uint8_t count){
  tft.writecommand(command); for(uint8_t i=0;i<count;i++) tft.writedata(data[i]);
}
void applyDisplayProfile(uint8_t profileIndex){
  const PanelProfile& profile=panelProfiles[profileIndex%8];
  const uint8_t gammaSelect=0x01;
  tft.startWrite();
  writePanelCommand(0x26,&gammaSelect,1);
  writePanelCommand(0xE0,profile.standardGamma?gammaStandardPositive:gammaLegacyPositive,15);
  writePanelCommand(0xE1,profile.standardGamma?gammaStandardNegative:gammaLegacyNegative,15);
  tft.writecommand(0x36);
  tft.writedata(0xE0 | (profile.rgbOrder?0x00:0x08));
  tft.endWrite();
  tft.invertDisplay(profile.inverted);
}
void settingsGear(int cx,int cy){
  for(int i=0;i<8;i++){ float a=i*0.785398f; int x1=cx+cos(a)*7,y1=cy+sin(a)*7,x2=cx+cos(a)*10,y2=cy+sin(a)*10; tft.drawLine(x1,y1,x2,y2,BLACK); }
  tft.drawCircle(cx,cy,7,BLACK); tft.drawCircle(cx,cy,3,BLACK); tft.fillCircle(cx,cy,1,BLACK);
}
void drawSettingsGear(){ if(!drawSdArt("/astral/settings_gear.jpg",225,213)) settingsGear(238,226); }
void drawBoot(){
  if(!drawSdArt("/astral/boot.jpg",0,0)){
    tft.fillScreen(INK); tft.drawRect(5,5,W-10,H-10,GOLD); tft.drawRect(10,10,W-20,H-20,BURG);
    center("THE",62,2,GOLD); center("ASTRAL",88,4,CREAM); center("CABINET",126,3,GOLD);
    center("a small instrument for reflection",174,1,MUTED);
  }
  doorPlaque(80,9,160,"MAXINE'S ASTRAL CABINET",NAVY);
  tft.setTextWrap(false,false); tft.setTextColor(CREAM); tft.setTextSize(1);
  const String prompt="TOUCH TO ENTER";
  tft.setCursor((W-tft.textWidth(prompt))/2,222); tft.print(prompt);
}
void drawJourney(){
  if(!drawSdArt("/astral/journey.jpg",0,0)){
    tft.fillScreen(INK); tft.drawRect(5,5,W-10,H-10,GOLD); tft.drawRect(10,10,W-20,H-20,BURG);
    tft.drawCircle(160,55,31,GOLD); tft.drawCircle(160,55,19,GOLD); tft.drawLine(129,55,191,55,GOLD); tft.drawLine(160,24,160,86,GOLD);
    tft.drawLine(160,87,64,142,GOLD); tft.drawLine(160,87,256,142,GOLD);
  }
  tft.setTextWrap(false,false); tft.setTextColor(GOLD); tft.setTextSize(2);
  const String title="CHOOSE YOUR JOURNEY";
  tft.setCursor((W-tft.textWidth(title))/2,104); tft.print(title);
  tft.setTextColor(CREAM); tft.setTextSize(1);
  const String prompt="Two paths open before you";
  tft.setCursor((W-tft.textWidth(prompt))/2,127); tft.print(prompt);
  button(65,145,190,30,"OFFLINE",BURG);
  button(65,185,190,30,"ONLINE",NAVY);
}
void drawOnlineSetup(){
  if(!drawSdArt("/astral/online.jpg",0,0)){
    tft.fillScreen(INK); tft.drawRect(5,5,W-10,H-10,GOLD); tft.drawRect(10,10,W-20,H-20,BURG);
    tft.drawCircle(160,54,28,GOLD); tft.drawCircle(160,54,14,GOLD); tft.drawLine(112,54,208,54,GOLD); tft.drawLine(160,14,160,94,GOLD);
  }
  button(14,16,32,15,"BACK",NAVY);
  overlayCenter("CONNECT TO THE POWER",96,2,GOLD);
  overlayCenter("OF THE UNIVERSE",117,2,GOLD);
  overlayCenter("1. CONNECT TO WI-FI: astral",150,1,CREAM);
  overlayCenter("2. OPEN: 192.168.4.1",168,1,CREAM);
  overlayCenter("3. CHOOSE YOUR HOME WI-FI",186,1,CREAM);
  overlayCenter("SAVE, THEN THE CABINET RESTARTS",204,1,MUTED);
}
void drawHome(){
  const String fourthDoor=internetReady ? "LIVE ASTRAL" : "ELEMENTAL RITUAL";
  if(!drawSdArt("/astral/doorways.jpg",0,0)){
    frame("MAXINE'S ASTRAL CABINET"); button(14,16,32,15,"BACK",NAVY); center("What would you like to consult?",72,1,CREAM); button(25,92,130,38,"DAILY OMEN"); button(165,92,130,38,"TAROT READING"); button(25,143,130,38,"ZODIAC"); button(165,143,130,38,fourthDoor); center("Touch a doorway to begin",202,1,MUTED);
    drawSettingsGear();
    return;
  }
  button(14,16,32,15,"BACK",NAVY);
  tft.setTextWrap(false,false); tft.setTextColor(CREAM); tft.setTextSize(1);
  doorPlaque(80,15,160,"MAXINE'S ASTRAL CABINET",NAVY);
  const String prompt="CHOOSE A DOORWAY";
  tft.setCursor((W-tft.textWidth(prompt))/2,34); tft.print(prompt);
  doorPlaque(30,105,100,"DAILY OMEN");
  doorPlaque(190,105,100,"TAROT READING",BURG);
  doorPlaque(35,188,90,"ZODIAC");
  doorPlaque(185,188,110,fourthDoor,BURG);
  drawSettingsGear();
}
struct ElementalRitual { const char* intention; const char* practice; const char* release; };
const ElementalRitual elementalRituals[4][3] = {
  { // Fire
    {"Name the spark you are ready to protect.","Light one small action before the day ends.","Release the need to make the first step perfect."},
    {"Choose one brave truth to say aloud.","Stand tall and take three slow breaths.","Let urgency become clear, steady purpose."},
    {"Notice where your energy wants to go.","Move your body for one song or one block.","Leave behind the task that is only noise."}},
  { // Earth
    {"Return to what is solid and already working.","Tend one useful thing with full attention.","Set down a burden that is not yours to carry."},
    {"Choose a promise your future self will feel.","Make your next step small, physical, and real.","Release the demand for immediate results."},
    {"Ask what needs patient care today.","Put both feet on the floor and breathe slowly.","Let enough be enough for this moment."}},
  { // Air
    {"Make room for the thought beneath the noise.","Write one honest sentence without editing it.","Release the answer you feel forced to give."},
    {"Follow the question that feels most alive.","Open a window or change your point of view.","Let an old story pass without an argument."},
    {"Name what you are curious to understand.","Share one kind, precise thought with someone.","Release the need to know every outcome."}},
  { // Water
    {"Honor the feeling asking to be witnessed.","Place a hand over your heart and soften your jaw.","Release the urge to explain your tenderness."},
    {"Notice what is quietly asking for compassion.","Drink water slowly and let the moment settle.","Let yesterday's emotion move on through you."},
    {"Ask where you need a gentler boundary.","Choose a small act that restores your inner space.","Release the feeling you no longer need to hold."}}
};
int selectedElement(){
  String element=String(signs[signIndex].element);
  if(element=="Fire") return 0;
  if(element=="Earth") return 1;
  if(element=="Air") return 2;
  return 3;
}
void drawElementalRitual(){
  if(!drawSdArt("/astral/elemental_ritual.jpg",0,0)) frame("ELEMENTAL RITUAL");
  button(14,16,32,15,"BACK",NAVY);
  const ElementalRitual& ritual=elementalRituals[selectedElement()][ritualVariant%3];
  const char* sections[]={"INTENTION","PRACTICE","RELEASE"};
  const char* contents[]={ritual.intention,ritual.practice,ritual.release};
  overlayAt("ELEMENTAL RITUAL",230,47,1,CREAM);
  overlayAt(String(signs[signIndex].name)+"  ·  "+signs[signIndex].element,230,63,1,GOLD);
  overlayAt(String(sections[ritualStep])+"  "+String(ritualStep+1)+" OF 3",230,82,1,GOLD);
  textWrap(contents[ritualStep],174,98,1,CREAM,112,6);
  overlayAt(ritualStep<2?"TOUCH TEXT TO CONTINUE":"TOUCH TEXT TO BEGIN AGAIN",230,190,1,CREAM);
  overlayAt("TOUCH EMBLEM FOR",74,204,1,CREAM);
  overlayAt("A NEW RITUAL",74,216,1,CREAM);
}
void drawSettings(){
  frame("SETTINGS");
  center("Cabinet preferences",57,1,MUTED);
  text("DISPLAY",28,78,1,GOLD);
  button(28,89,264,27,"DISPLAY CALIBRATION",NAVY);
  text("NETWORK",28,135,1,GOLD);
  text("Remove the saved Wi-Fi network and",28,149,1,CREAM);
  text("return to first-time setup.",28,162,1,CREAM);
  button(28,181,264,28,"DELETE SAVED WI-FI",BURG);
}
void drawDisplayCalibration(){
  applyDisplayProfile(previewDisplayProfile);
  frame("DISPLAY CALIBRATION");
  center(String("PROFILE ")+String(previewDisplayProfile+1)+" OF 8",50,1,GOLD);
  center(panelProfiles[previewDisplayProfile].name,63,1,CREAM);
  tft.fillRect(31,81,56,35,0xF800); tft.fillRect(96,81,56,35,0x07E0); tft.fillRect(161,81,56,35,0x001F); tft.fillRect(226,81,56,35,0xFFFF);
  overlayAt("RED",59,121,1,CREAM); overlayAt("GREEN",124,121,1,CREAM); overlayAt("BLUE",189,121,1,CREAM); overlayAt("WHITE",254,121,1,CREAM);
  center("Choose rich colors and a deep navy background.",138,1,CREAM);
  center("Avoid a washed-out blue-white image.",150,1,CREAM);
  button(28,174,120,28,"TRY NEXT",NAVY);
  button(172,174,120,28,"SAVE THIS",BURG);
}
void drawConfirmNetworkDelete(){
  frame("DELETE SAVED WI-FI?");
  center("This cannot be undone.",62,1,GOLD);
  textWrap("The cabinet will forget its saved Wi-Fi network and online setup details, then restart.",30,86,1,CREAM,260,4);
  button(28,165,118,30,"CANCEL",NAVY);
  button(174,165,118,30,"DELETE & REBOOT",BURG);
}
void zodiacMark(uint8_t id,int cx,int cy,int r,uint16_t color);
void drawZodiacTile(uint8_t id,int x,int y){
  tft.fillRoundRect(x-2,y-2,64,64,6,id==signIndex?BURG:NAVY);
  if(!drawZodiacArt(id,x,y)) zodiacMark(id,x+30,y+28,18,GOLD);
  tft.fillRect(x,y+48,60,12,INK);
  tft.setTextSize(1); int labelX=x+(60-tft.textWidth(signs[id].name))/2;
  text(signs[id].name,labelX,y+50,1,id==signIndex?CREAM:GOLD);
  tft.drawRoundRect(x-2,y-2,64,64,6,id==signIndex?CREAM:GOLD);
}
void drawZodiac(){ frame("CHOOSE YOUR SIGN"); for(int i=0;i<12;i++){ int col=i%4,row=i/4; drawZodiacTile(i,10+col*80,45+row*60); } }
void drawMenu(){
  if(!drawSdArt("/astral/reading_menu.jpg",0,0)){ frame("SELECT A READING"); }
  button(14,16,32,15,"BACK",NAVY);
  overlayAt(String(signs[signIndex].name),160,48,1,CREAM);
  overlayAt("ONE-CARD",60,146,1,CREAM); overlayAt("DAILY OMEN",60,160,1,CREAM);
  overlayAt("THREE-CARD",258,146,1,CREAM); overlayAt("READING",258,160,1,CREAM);
  overlayAt("CHANGE SIGN",160,175,1,CREAM);
}
String moonPhase(){ return String(lunarInfo().phase); }

void star(int cx,int cy,int r,uint16_t color){
  tft.drawLine(cx,cy-r,cx+r/3,cy+r/3,color); tft.drawLine(cx+r/3,cy+r/3,cx-r,cy-r/4,color);
  tft.drawLine(cx-r,cy-r/4,cx+r,cy-r/4,color); tft.drawLine(cx+r,cy-r/4,cx-r/3,cy+r/3,color);
  tft.drawLine(cx-r/3,cy+r/3,cx,cy-r,color);
}
void sunGlyph(int cx,int cy,int r,uint16_t color){ tft.drawCircle(cx,cy,r,color); tft.fillCircle(cx,cy,2,color); for(int i=0;i<8;i++){ float a=i*0.785398f; int x1=cx+cos(a)*(r+3),y1=cy+sin(a)*(r+3); int x2=cx+cos(a)*(r+7),y2=cy+sin(a)*(r+7); tft.drawLine(x1,y1,x2,y2,color); } }
void moonGlyph(int cx,int cy,int r,uint16_t color){ tft.fillCircle(cx,cy,r,color); tft.fillCircle(cx+r/2,cy-r/3,r,color==GOLD?NAVY:INK); }
void cardMotif(uint8_t id,int cx,int cy,int s,uint16_t color){
  switch(id){
    case 0: tft.drawLine(cx-s,cy+s,cx,cy-s,color); tft.drawLine(cx,cy-s,cx+s,cy+s,color); sunGlyph(cx,cy-s/2,s/4,color); break;
    case 1: tft.drawLine(cx,cy-s,cx,cy+s,color); tft.drawCircle(cx,cy-s,s/7,color); star(cx-s/2,cy,s/5,color); star(cx+s/2,cy,s/5,color); break;
    case 2: moonGlyph(cx,cy-s/3,s/4,color); tft.drawRect(cx-s/2,cy-s/2,s/5,s,color); tft.drawRect(cx+s/3,cy-s/2,s/5,s,color); break;
    case 3: tft.drawLine(cx,cy+s,cx,cy-s/3,color); tft.drawCircle(cx,cy-s/2,s/3,color); for(int i=0;i<6;i++) tft.drawCircle(cx+cos(i*1.047f)*s/3,cy-s/2+sin(i*1.047f)*s/3,s/7,color); break;
    case 4: tft.drawLine(cx-s,cy+s/2,cx+s,cy+s/2,color); tft.drawLine(cx-s/2,cy+s/2,cx-s/3,cy-s/2,color); tft.drawLine(cx-s/3,cy-s/2,cx,cy-s/4,color); tft.drawLine(cx,cy-s/4,cx+s/3,cy-s/2,color); tft.drawLine(cx+s/3,cy-s/2,cx+s/2,cy+s/2,color); break;
    case 5: tft.drawRect(cx-s/2,cy-s/3,s,s*2/3,color); tft.drawTriangle(cx-s*2/3,cy-s/3,cx,cy-s,cx+s*2/3,cy-s/3,color); tft.drawLine(cx,cy-s,cx,cy+s,color); break;
    case 6: tft.drawCircle(cx-s/4,cy,s/4,color); tft.drawCircle(cx+s/4,cy,s/4,color); tft.drawLine(cx-s/2,cy,cx,cy+s/2,color); tft.drawLine(cx+s/2,cy,cx,cy+s/2,color); star(cx,cy-s/2,s/5,color); break;
    case 7: tft.drawCircle(cx-s/2,cy+s/3,s/4,color); tft.drawCircle(cx+s/2,cy+s/3,s/4,color); tft.drawRect(cx-s/2,cy-s/3,s,s/2,color); tft.drawLine(cx,cy-s/3,cx,cy-s,color); break;
    case 8: tft.drawCircle(cx,cy,s/2,color); tft.drawLine(cx-s/3,cy-s/4,cx-s/2,cy-s/2,color); tft.drawLine(cx+s/3,cy-s/4,cx+s/2,cy-s/2,color); tft.drawCircle(cx-s/5,cy-s/8,2,color); tft.drawCircle(cx+s/5,cy-s/8,2,color); break;
    case 9: tft.drawCircle(cx,cy-s/4,s/3,color); tft.drawLine(cx-s/3,cy+s/2,cx,cy-s/4,color); tft.drawLine(cx,cy-s/4,cx+s/3,cy+s/2,color); sunGlyph(cx,cy-s/2,s/5,color); break;
    case 10: tft.drawCircle(cx,cy,s/2,color); for(int i=0;i<8;i++){ float a=i*0.785398f; tft.drawLine(cx,cy,cx+cos(a)*s/2,cy+sin(a)*s/2,color); } break;
    case 11: tft.drawLine(cx,cy-s/2,cx,cy+s/2,color); tft.drawLine(cx-s/2,cy-s/3,cx+s/2,cy-s/3,color); tft.drawLine(cx-s/2,cy-s/3,cx-s/3,cy+s/4,color); tft.drawLine(cx+s/2,cy-s/3,cx+s/3,cy+s/4,color); tft.drawCircle(cx-s/3,cy+s/3,s/5,color); tft.drawCircle(cx+s/3,cy+s/3,s/5,color); break;
    case 12: tft.drawLine(cx,cy-s,cx,cy+s,color); tft.drawCircle(cx,cy-s/2,s/5,color); tft.drawLine(cx-s/2,cy+s/3,cx+s/2,cy+s/3,color); break;
    case 13: tft.drawCircle(cx,cy,s/2,color); tft.drawCircle(cx-s/5,cy-s/8,2,color); tft.drawCircle(cx+s/5,cy-s/8,2,color); tft.drawLine(cx-s/4,cy+s/5,cx+s/4,cy+s/5,color); break;
    case 14: tft.drawCircle(cx-s/3,cy,s/4,color); tft.drawCircle(cx+s/3,cy,s/4,color); tft.drawLine(cx-s/3,cy+s/4,cx,cy+s/2,color); tft.drawLine(cx+s/3,cy+s/4,cx,cy+s/2,color); break;
    case 15: tft.drawCircle(cx-s/4,cy,s/3,color); tft.drawCircle(cx+s/4,cy,s/3,color); tft.drawLine(cx-s/2,cy-s/3,cx-s/3,cy-s,color); tft.drawLine(cx+s/2,cy-s/3,cx+s/3,cy-s,color); break;
    case 16: tft.drawRect(cx-s/3,cy-s/2,2*s/3,s,color); tft.drawLine(cx-s/2,cy-s,cx+s/2,cy+s,color); tft.drawLine(cx+s/2,cy-s,cx-s/2,cy+s,color); break;
    case 17: star(cx,cy,s/2,color); star(cx-s/2,cy+s/3,s/5,color); star(cx+s/2,cy+s/3,s/5,color); break;
    case 18: moonGlyph(cx,cy,s/2,color); star(cx+s/2,cy-s/3,s/6,color); star(cx-s/2,cy+s/3,s/6,color); break;
    case 19: sunGlyph(cx,cy,s/2,color); tft.drawPixel(cx-s/5,cy-s/8,color); tft.drawPixel(cx+s/5,cy-s/8,color); break;
    case 20: tft.drawCircle(cx,cy,s/3,color); tft.drawLine(cx+s/3,cy,cx+s,cy-s/2,color); tft.drawLine(cx+s/3,cy,cx+s,cy+s/2,color); star(cx-s/2,cy,s/5,color); break;
    case 21: tft.drawCircle(cx,cy,s/2,color); tft.drawCircle(cx,cy,s/3,color); tft.drawLine(cx-s/2,cy,cx+s/2,cy,color); tft.drawLine(cx,cy-s/2,cx,cy+s/2,color); star(cx,cy,s/6,color); break;
    default: tft.drawCircle(cx,cy,s/2,color); tft.drawCircle(cx,cy,s/3,color); star(cx,cy,s/6,color); break;
  }
}
void drawCardArt(uint8_t id,int x,int y,int w,int h,bool mini=false,bool reversed=false){
  tft.fillRoundRect(x-2,y-2,w+4,h+4,5,BURG);
  if(!drawSdCard(id,x,y,mini,reversed)){
    tft.fillRoundRect(x,y,w,h,5,NAVY);
    cardMotif(id,x+w/2,y+h/2,mini?min(w,h)/3:min(w,h)/4,GOLD);
  }
  tft.drawRoundRect(x-2,y-2,w+4,h+4,5,GOLD);
  if(!mini && !sdArtReady){ text(cards[id].name,x+6,y+h-18,1,CREAM); }
}
void drawCardBack(int x,int y,int w,int h){
  tft.fillRoundRect(x-2,y-2,w+4,h+4,5,BURG);
  if(!drawSdCardBack(x,y,true)){
    tft.fillRoundRect(x,y,w,h,5,NAVY);
    star(x+w/2,y+h/2,min(w,h)/3,GOLD);
  }
  tft.drawRoundRect(x-2,y-2,w+4,h+4,5,GOLD);
}
void zodiacMark(uint8_t id,int cx,int cy,int r,uint16_t color){
  tft.drawCircle(cx,cy,r+2,color); cardMotif(id%12,cx,cy,r,color);
}
void drawDaily(){
  frame("DAILY OMEN"); const Card&c=cards[drawn[0]];
  if(reveal==0){
    center(String(signs[signIndex].name)+"  ·  "+moonPhase(),51,1,GOLD);
    drawCardBack(120,70,78,104);
    center("TOUCH THE DECK TO DRAW",184,1,CREAM);
    center(sdArtReady?"the cabinet remembers its pictures":"insert art SD card for illustrated deck",201,1,MUTED);
  } else {
    drawCardArt(drawn[0],12,54,120,160,false,reversed[0]);
    panelTitle(c.name,146,72,158);
    text(reversed[0]?"REVERSED":"UPRIGHT",146,96,1,reversed[0]?BURG:GOLD);
    textWrap(reversed[0]?c.rev:c.up,146,116,1,CREAM,158,7);
    text("touch below for another omen",146,202,1,MUTED);
  }
  footer();
}
void textWrap(const String&s,int x,int y,int size,uint16_t color,int width,int maxLines){
  tft.setTextWrap(false,false); tft.setTextSize(size);
  String word,line; int yy=y, lineCount=0;
  auto emit=[&](const String&value){
    if(!value.length() || (maxLines && lineCount>=maxLines)) return false;
    text(value,x,yy,size,color); yy+=11*size; lineCount++;
    return !maxLines || lineCount<maxLines;
  };
  for(unsigned i=0;i<=s.length();i++){
    char ch=i<s.length()?s[i]:' ';
    if(ch==' '){
      if(!word.length()) continue;
      String candidate=line.length()?line+" "+word:word;
      if(line.length() && tft.textWidth(candidate)>width){ if(!emit(line)) return; line=word; }
      else line=candidate;
      word="";
    } else word+=ch;
  }
  emit(line);
}
void drawSpread(){ frame("THREE CARD READING"); center(String(signs[signIndex].name)+"  ·  "+(reveal<3?"reveal the cards":"your constellation of meaning"),51,1,GOLD); const char* pos[]={"PAST","PRESENT","BECOMING"}; for(int i=0;i<3;i++){ int x=20+i*100; if(i>=reveal){ drawCardBack(x,76,78,104); } else { drawCardArt(drawn[i],x,76,78,104,true,reversed[i]); } text(pos[i],x+15,188,1,GOLD); } if(reveal==3) center("Tap a card to read its meaning",204,1,MUTED); footer(); }
void drawCardDetail(){
  const Card&c=cards[drawn[detailCard]];
  const char* pos[]={"PAST", "PRESENT", "BECOMING"};
  frame(String(pos[detailCard])+" CARD");
  drawCardArt(drawn[detailCard],12,54,120,160,false,reversed[detailCard]);
  panelTitle(c.name,146,72,158);
  text(reversed[detailCard]?"REVERSED":"UPRIGHT",146,96,1,reversed[detailCard]?BURG:GOLD);
  textWrap(reversed[detailCard]?c.rev:c.up,146,116,1,CREAM,158,7);
  text("tap to return to spread",146,202,1,MUTED);
  footer();
}
void drawCabinet(){ frame("THE CABINET"); center("A little archive of the self",50,1,MUTED); text("GUEST",28,76,1,GOLD); text(guestName,110,76,2,CREAM); button(28,98,82,25,"GUEST"); button(119,98,82,25,"MOON CHILD",NAVY); button(210,98,82,25,"STAR SEEKER",NAVY); text("SIGN",28,136,1,GOLD); text(signs[signIndex].name,110,136,2,CREAM); text("ELEMENT",28,164,1,GOLD); text(signs[signIndex].element,110,164,1,CREAM); text("MOON",28,184,1,GOLD); text(moonPhase(),110,184,1,CREAM); button(20,195,280,28,"RETURN TO RITUAL",BURG); }
void drawLiveAstral(){
  if(!drawSdArt("/astral/live_astral.jpg",0,0)){ frame("LIVE ASTRAL"); }
  button(14,16,32,15,"BACK",NAVY);
  overlayCenter("LIVE ASTRAL",46,2,CREAM);
  overlayAt("SKY NOW",55,164,1,CREAM);
  overlayAt("RITUAL",160,164,1,CREAM); overlayAt("CALENDAR",160,177,1,CREAM);
  overlayAt("COSMIC",266,164,1,CREAM); overlayAt("WEATHER",266,177,1,CREAM);
}
void drawSkyNow(){
  if(!drawSdArt("/astral/sky_now.jpg",0,0)){ frame("SKY NOW"); }
  button(14,16,32,15,"BACK",NAVY);
  LunarInfo moon=lunarInfo();
  text(localDateTime(),151,48,1,GOLD);
  String phase=moon.phase; int illumination=moon.illumination; float nextFull=moon.nextFull, nextNew=moon.nextNew;
  if(cosmicCacheCurrent()){
    String cachedMoon=cosmicLine(2); cachedMoon.remove(0,6);
    int percent=cachedMoon.lastIndexOf(' ');
    if(percent>0){ phase=cachedMoon.substring(0,percent); illumination=cachedMoon.substring(percent+1).toInt(); }
    float cachedFull=cachedEventDays(3), cachedNew=cachedEventDays(4);
    if(cachedFull>=0) nextFull=cachedFull;
    if(cachedNew>=0) nextNew=cachedNew;
  }
  phase.toUpperCase();
  text(phase,151,70,1,CREAM);
  if(!clockSynchronized){
    text("Awaiting time signal",151,99,1,MUTED);
    text("Keep Wi-Fi connected",151,116,1,MUTED);
  } else {
    text(String(illumination)+"% illuminated",151,96,1,GOLD);
    text("NEXT FULL",151,127,1,GOLD); text(lunarCountdown(nextFull),226,127,1,CREAM);
    text("NEXT NEW",151,151,1,GOLD); text(lunarCountdown(nextNew),226,151,1,CREAM);
  }
}
void drawRitualCalendar(){
  if(!drawSdArt("/astral/ritual_calendar.jpg",0,0)){ frame("RITUAL CALENDAR"); }
  button(14,16,32,15,"BACK",NAVY);
  LunarInfo moon=lunarInfo();
  float nextNew=moon.nextNew, nextFull=moon.nextFull;
  if(cosmicCacheCurrent()){
    float cachedNew=cachedEventDays(4), cachedFull=cachedEventDays(3);
    if(cachedNew>=0) nextNew=cachedNew;
    if(cachedFull>=0) nextFull=cachedFull;
  }
  overlayAt("RITUAL CALENDAR",160,61,1,GOLD);
  if(!clockSynchronized){ overlayAt("The calendar opens",160,104,1,CREAM); overlayAt("when time arrives",160,119,1,CREAM); }
  else {
    overlayAt("NEW MOON",160,88,1,GOLD); overlayAt(lunarCountdown(nextNew),160,103,1,CREAM);
    overlayAt("FULL MOON",160,126,1,GOLD); overlayAt(lunarCountdown(nextFull),160,141,1,CREAM);

  }
}
void drawCosmicWeather(){
  if(!drawSdArt("/astral/cosmic_weather.jpg",0,0)){ frame("COSMIC WEATHER"); }
  button(14,16,32,15,"BACK",NAVY);
  String header=cosmicLine(0);
  if(header!="ASTRAL COSMIC WEATHER"){
    text("COSMIC WEATHER",145,51,1,GOLD);
    text("No celestial cache yet.",145,102,1,CREAM);
    text("Return after connecting.",145,120,1,MUTED);
  } else {
    text("COSMIC WEATHER",145,51,1,GOLD);
    text(cosmicLine(1),145,69,1,MUTED);
    int y=92;
    for(uint8_t i=5;i<10;i++){
      String line=cosmicLine(i); if(!line.length()) continue;
      textWrap(line,145,y,1,CREAM,128,1); y+=22;
    }
  }
}
void newReading(bool three){ reveal=0; for(int i=0;i<3;i++){ drawn[i]=random(22); reversed[i]=random(100)<25; } screen=three?SPREAD:DAILY; }
void tap(int x,int y){
 if(screen==BOOT){ internetReady=internetAvailable(savedWifiSsid.length()?3500:0); if(internetReady) requestClockSync(); screen=internetReady?HOME:JOURNEY; uiRevision++; return; }
 if(screen==JOURNEY){ if(x>=65 && x<255 && y>=145 && y<175)screen=HOME; else if(x>=65 && x<255 && y>=185 && y<215){ startConfigPortal(); screen=ONLINE_SETUP; } uiRevision++; return; }
 if(screen==ONLINE_SETUP){ if(x>=0 && x<65 && y>=0 && y<50){ stopConfigPortal(); screen=JOURNEY; } uiRevision++; return; }
 if(screen==DISPLAY_CALIBRATION && x>=0 && x<65 && y>=0 && y<50){ previewDisplayProfile=displayProfile; applyDisplayProfile(displayProfile); screen=SETTINGS; uiRevision++; return; }
 if(screen!=HOME && x>=0 && x<65 && y>=0 && y<50){ screen=(screen==SKY_NOW||screen==RITUAL_CALENDAR||screen==COSMIC_WEATHER)?LIVE_ASTRAL:(screen==CONFIRM_NETWORK_DELETE?SETTINGS:HOME); uiRevision++; return; }
 if(screen==HOME){ if(x>=0 && x<65 && y>=0 && y<50)screen=JOURNEY; else if(x>=214&&x<263&&y>=213&&y<240)screen=SETTINGS; else if(x>=10&&x<160&&y>=55&&y<130)newReading(false); else if(x>=160&&x<310&&y>=55&&y<130)newReading(true); else if(x>=10&&x<160&&y>=133&&y<212)screen=ZODIAC; else if(x>=160&&x<310&&y>=133&&y<212){ if(internetReady)screen=LIVE_ASTRAL; else { ritualStep=0; screen=ELEMENTAL_RITUAL; } } }
 else if(screen==SETTINGS){ if(x>=28&&x<292&&y>=89&&y<116){ previewDisplayProfile=displayProfile; screen=DISPLAY_CALIBRATION; } else if(x>=28&&x<292&&y>=181&&y<209)screen=CONFIRM_NETWORK_DELETE; }
 else if(screen==DISPLAY_CALIBRATION){
   if(x>=28&&x<148&&y>=174&&y<202) previewDisplayProfile=(previewDisplayProfile+1)%8;
   else if(x>=172&&x<292&&y>=174&&y<202){ displayProfile=previewDisplayProfile; prefs.putUChar("displayProfile",displayProfile); screen=SETTINGS; }
 }
 else if(screen==CONFIRM_NETWORK_DELETE){
   if(x>=28&&x<146&&y>=165&&y<195)screen=SETTINGS;
   else if(x>=174&&x<292&&y>=165&&y<195){
     Serial.println("ASTRAL: deleting saved Wi-Fi and online setup");
     WiFi.disconnect(true,true);
     netPrefs.clear();
     savedWifiSsid=""; savedWifiPassword=""; savedZipcode=""; skyLocationSet=false; internetReady=false;
     delay(200); ESP.restart(); return;
   }
 }
 else if(screen==ELEMENTAL_RITUAL){
   if(x<145&&y>=45&&y<230){ ritualVariant=(ritualVariant+1)%3; ritualStep=0; }
   else if(x>=145&&x<310&&y>=55&&y<210) ritualStep=(ritualStep+1)%3;
 }
 else if(screen==LIVE_ASTRAL){ if(y>=130&&y<205&&x<105){ refreshCosmicCache(); screen=SKY_NOW; } else if(y>=130&&y<205&&x<215)screen=RITUAL_CALENDAR; else if(y>=130&&y<205){ refreshCosmicCache(); screen=COSMIC_WEATHER; } }
 else if(screen==ZODIAC){ if(y>=45&&y<225){ int col=(x-10)/80,row=(y-45)/60; if(col>=0&&col<4&&row>=0&&row<3&&x>=10+col*80&&x<70+col*80){ signIndex=row*4+col; screen=MENU; } } }
 else if(screen==MENU){ if(x>=10&&x<112&&y>=130&&y<180)newReading(false); else if(x>=208&&x<310&&y>=130&&y<180)newReading(true); else if(x>=110&&x<210&&y>=160&&y<200)screen=ZODIAC; }
 else if(screen==DAILY){ if(x<45&&y<45)screen=HOME; else { reveal=1; if(y>180)newReading(false); } }
 else if(screen==SPREAD){ if(x<45&&y<45)screen=HOME; else if(reveal<3)reveal++; else if(y>=70&&y<190&&x>=20&&x<298){ detailCard=constrain((x-20)/100,0,2); screen=CARD; } else if(y>195)newReading(true); }
 else if(screen==CARD){ screen=SPREAD; }
 else if(screen==CABINET){ if(y>=190)screen=HOME; else if(y>95&&y<130){ if(x<112)guestName="Astral Guest"; else if(x<207)guestName="Moon Child"; else guestName="Star Seeker"; prefs.putString("name",guestName); } }
 uiRevision++;
}
void setup(){
  Serial.begin(115200);
  delay(300);
  Serial.println("ASTRAL: setup start");
  randomSeed(analogRead(34));
  Serial.println("ASTRAL: random ready");
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  Serial.println("ASTRAL: backlight on");
  tft.init();
  Serial.println("ASTRAL: tft init complete");
  tft.setRotation(3);
  Serial.printf("ASTRAL: dimensions %d x %d\n", tft.width(), tft.height());
  Serial.println("ASTRAL: rotation complete");
  sdSPI.begin(18, 19, 23, 5);
  sdArtReady=SD.begin(5,sdSPI);
  if(sdArtReady){
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tftOutput);
    Serial.println("ASTRAL: SD art ready");
  } else {
    Serial.println("ASTRAL: SD art unavailable; using vector fallback");
  }
  touchSPI.begin(25, 39, 32, 33);
  touch.begin(touchSPI);
  touch.setRotation(3);
  Serial.println("ASTRAL: touch init complete");
  prefs.begin("cabinet",false);
  guestName=prefs.getString("name",DEFAULT_NAME);
  displayProfile=prefs.getUChar("displayProfile",0)%8;
  previewDisplayProfile=displayProfile;
  applyDisplayProfile(displayProfile);
  Serial.println("ASTRAL: preferences complete");
  netPrefs.begin("network",false);
  connectSavedWifi();
  Serial.println("ASTRAL: network preferences complete");
  drawBoot();
  Serial.println("ASTRAL: boot art complete");
}
void loop(){
  maintainClock();
  if(portalActive) webServer.handleClient();
  if(touch.touched()){
    TS_Point p=touch.getPoint();
    int x=map(p.x,TOUCH_X_MIN,TOUCH_X_MAX,0,W);
    int y=map(p.y,TOUCH_Y_MIN,TOUCH_Y_MAX,0,H);
    x=constrain(x,0,W-1);
    y=constrain(y,0,H-1);
    Serial.printf("ASTRAL: touch raw=%d,%d,%d mapped=%d,%d\\n", p.x,p.y,p.z,x,y);
    tap(x,y);
    while(touch.touched())delay(10);
    delay(120);
  }
  static Screen last=BOOT;
  static int lastReveal=-1;
  static uint32_t lastUiRevision=UINT32_MAX;
  if(last!=screen||lastReveal!=reveal||lastUiRevision!=uiRevision){
    if(screen==BOOT)drawBoot();
    else if(screen==JOURNEY)drawJourney();
    else if(screen==ONLINE_SETUP)drawOnlineSetup();
    else if(screen==HOME)drawHome();
    else if(screen==SETTINGS)drawSettings();
    else if(screen==DISPLAY_CALIBRATION)drawDisplayCalibration();
    else if(screen==CONFIRM_NETWORK_DELETE)drawConfirmNetworkDelete();
    else if(screen==ELEMENTAL_RITUAL)drawElementalRitual();
    else if(screen==ZODIAC)drawZodiac();
    else if(screen==MENU)drawMenu();
    else if(screen==DAILY)drawDaily();
    else if(screen==SPREAD)drawSpread();
    else if(screen==CARD)drawCardDetail();
    else if(screen==CABINET)drawCabinet();
    else if(screen==LIVE_ASTRAL)drawLiveAstral();
    else if(screen==SKY_NOW)drawSkyNow();
    else if(screen==RITUAL_CALENDAR)drawRitualCalendar();
    else if(screen==COSMIC_WEATHER)drawCosmicWeather();
    last=screen;
    lastReveal=reveal;
    lastUiRevision=uiRevision;
  }
}
