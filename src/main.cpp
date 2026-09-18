#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <time.h>
#include <SD.h>
#include <TJpg_Decoder.h>

TFT_eSPI tft(240, 320);
SPIClass touchSPI = SPIClass(VSPI);
SPIClass sdSPI = SPIClass(HSPI);
XPT2046_Touchscreen touch(33, 36);
Preferences prefs;
bool sdArtReady = false;

constexpr int W=320, H=240;
constexpr int TOUCH_X_MIN=200, TOUCH_X_MAX=3900, TOUCH_Y_MIN=200, TOUCH_Y_MAX=3900;
const uint16_t INK=0x0841, NAVY=0x10A3, BURG=0x780C, GOLD=0xD5A5, CREAM=0xFFDB, MUTED=0xB5B6, BLACK=0x0000;
const char* DEFAULT_NAME="Astral Guest";

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

enum Screen { BOOT, HOME, ZODIAC, MENU, DAILY, SPREAD, CARD, CABINET };
Screen screen=BOOT; int signIndex=0, spreadMode=0, reveal=0, detailCard=0; int drawn[3]={0,0,0}; bool reversed[3]={false,false,false}; String guestName;
uint32_t uiRevision=0;
void textWrap(const String&s,int x,int y,int size,uint16_t color,int width,int maxLines=0);

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
bool drawSdCardBack(int x, int y, bool thumbnail=false){ return drawSdArt(thumbnail ? "/astral/card_back_s.jpg" : "/astral/card_back.jpg", x, y); }
String zodiacArtPath(uint8_t id){ char path[32]; snprintf(path, sizeof(path), "/astral/zodiac/%02u.jpg", id); return String(path); }
bool drawZodiacArt(uint8_t id, int x, int y){ return drawSdArt(zodiacArtPath(id), x, y); }

void text(const String&s,int x,int y,int size=1,uint16_t c=CREAM){ tft.setTextWrap(false,false); tft.setTextColor(c,INK); tft.setTextSize(size); tft.setCursor(x,y); tft.print(s); }
void center(const String&s,int y,int size=1,uint16_t c=CREAM){ tft.setTextSize(size); int x=(W-tft.textWidth(s))/2; text(s,x,y,size,c); }
void panelTitle(const String&s,int x,int y,int width){ tft.setTextSize(2); text(s,x,y,tft.textWidth(s)<=width?2:1,CREAM); }
void frame(const String&title){
  tft.fillScreen(INK); tft.drawRect(5,5,W-10,H-10,GOLD); tft.drawRect(10,10,W-20,H-20,BURG);
  center(title,16,2,GOLD); tft.drawFastHLine(25,40,W-50,GOLD);
  if(screen!=HOME){
    constexpr int homeX=14, homeY=15, homeW=36, homeH=17;
    tft.fillRoundRect(homeX,homeY,homeW,homeH,4,NAVY);
    tft.drawRoundRect(homeX,homeY,homeW,homeH,4,GOLD);
    tft.setTextWrap(false,false); tft.setTextColor(CREAM,NAVY); tft.setTextSize(1);
    tft.setCursor(homeX+(homeW-tft.textWidth("HOME"))/2,homeY+(homeH-8)/2);
    tft.print("HOME");
  }
}
void button(int x,int y,int w,int h,const String&label,uint16_t fill=BURG){ tft.fillRoundRect(x,y,w,h,6,fill); tft.drawRoundRect(x,y,w,h,6,GOLD); tft.setTextWrap(false,false); tft.setTextColor(CREAM,fill); tft.setTextSize(1); int tx=x+(w-tft.textWidth(label))/2; tft.setCursor(tx,y+(h-8)/2); tft.print(label); }
void footer(){ text("THE ASTRAL CABINET",92,222,1,MUTED); }
void drawBoot(){
  if(!drawSdArt("/astral/boot.jpg",0,0)){
    tft.fillScreen(INK); tft.drawRect(5,5,W-10,H-10,GOLD); tft.drawRect(10,10,W-20,H-20,BURG);
    center("THE",62,2,GOLD); center("ASTRAL",88,4,CREAM); center("CABINET",126,3,GOLD);
    center("a small instrument for reflection",174,1,MUTED);
  }
  tft.setTextWrap(false,false); tft.setTextColor(CREAM); tft.setTextSize(1);
  const String prompt="TOUCH TO ENTER";
  tft.setCursor((W-tft.textWidth(prompt))/2,222); tft.print(prompt);
}
void drawHome(){ frame("THE ASTRAL CABINET"); center("Welcome, "+guestName,54,1,MUTED); center("What would you like to consult?",72,1,CREAM); button(25,92,130,38,"DAILY OMEN"); button(165,92,130,38,"TAROT READING"); button(25,143,130,38,"ZODIAC"); button(165,143,130,38,"CABINET"); center("Touch a doorway to begin",202,1,MUTED); }
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
void drawMenu(){ frame("SELECT A READING"); center(String(signs[signIndex].name)+"  /  "+signs[signIndex].element,53,1,GOLD); button(30,75,260,34,"ONE-CARD DAILY OMEN"); button(30,120,260,34,"PAST / PRESENT / BECOMING"); button(30,165,125,30,"CHANGE SIGN",NAVY); button(165,165,125,30,"CABINET",NAVY); footer(); }
String moonPhase(){ time_t now=time(nullptr); if(now<100000) return "the moon keeps its own time"; long days=(now/86400L)-10957; int phase=(days%30+30)%30; if(phase<2) return "new moon"; if(phase<7) return "waxing crescent"; if(phase<9) return "first quarter"; if(phase<14) return "waxing gibbous"; if(phase<16) return "full moon"; if(phase<22) return "waning gibbous"; if(phase<24) return "last quarter"; return "waning crescent"; }

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
void newReading(bool three){ reveal=0; for(int i=0;i<3;i++){ drawn[i]=random(22); reversed[i]=random(100)<25; } screen=three?SPREAD:DAILY; }
void tap(int x,int y){
 if(screen==BOOT){ screen=HOME; uiRevision++; return; }
 if(screen!=HOME && x<65 && y<48){ screen=HOME; uiRevision++; return; }
 if(screen==HOME){ if(y>88&&y<135){ if(x<160)newReading(false); else newReading(true); } else if(y>140&&y<187){ if(x<160)screen=ZODIAC; else screen=CABINET; } }
 else if(screen==ZODIAC){ if(y>=45&&y<225){ int col=(x-10)/80,row=(y-45)/60; if(col>=0&&col<4&&row>=0&&row<3&&x>=10+col*80&&x<70+col*80){ signIndex=row*4+col; screen=MENU; } } }
 else if(screen==MENU){ if(y>70&&y<115)newReading(false); else if(y>115&&y<160)newReading(true); else if(y>160&&x<160)screen=ZODIAC; else if(y>160)screen=CABINET; }
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
  tft.invertDisplay(true);
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
  Serial.println("ASTRAL: preferences complete");
  drawBoot();
  Serial.println("ASTRAL: boot art complete");
}
void loop(){
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
    else if(screen==HOME)drawHome();
    else if(screen==ZODIAC)drawZodiac();
    else if(screen==MENU)drawMenu();
    else if(screen==DAILY)drawDaily();
    else if(screen==SPREAD)drawSpread();
    else if(screen==CARD)drawCardDetail();
    else if(screen==CABINET)drawCabinet();
    last=screen;
    lastReveal=reveal;
    lastUiRevision=uiRevision;
  }
}
