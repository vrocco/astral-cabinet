#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <time.h>

TFT_eSPI tft;
XPT2046_Touchscreen touch(33, 36);
Preferences prefs;

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

enum Screen { HOME, ZODIAC, MENU, DAILY, SPREAD, CARD, CABINET };
Screen screen=HOME; int signIndex=0, spreadMode=0, reveal=0; int drawn[3]={0,0,0}; bool reversed[3]={false,false,false}; String guestName;
void textWrap(const String&s,int x,int y,int size,uint16_t color,int width);

void text(const String&s,int x,int y,int size=1,uint16_t c=CREAM){ tft.setTextColor(c,INK); tft.setTextSize(size); tft.setCursor(x,y); tft.print(s); }
void center(const String&s,int y,int size=1,uint16_t c=CREAM){ tft.setTextSize(size); int x=(W-tft.textWidth(s))/2; text(s,x,y,size,c); }
void frame(const String&title){ tft.fillScreen(INK); tft.drawRect(5,5,W-10,H-10,GOLD); tft.drawRect(10,10,W-20,H-20,BURG); center(title,16,2,GOLD); tft.drawFastHLine(25,40,W-50,GOLD); }
void button(int x,int y,int w,int h,const String&label,uint16_t fill=BURG){ tft.fillRoundRect(x,y,w,h,6,fill); tft.drawRoundRect(x,y,w,h,6,GOLD); tft.setTextSize(1); int tx=x+(w-tft.textWidth(label))/2; text(label,tx,y+(h-8)/2,1,CREAM); }
void footer(){ text("<",14,18,2,GOLD); text("THE ASTRAL CABINET",92,222,1,MUTED); }
void splash(){ tft.fillScreen(INK); center("THE",62,2,GOLD); center("ASTRAL",88,4,CREAM); center("CABINET",126,3,GOLD); center("a small instrument for reflection",174,1,MUTED); delay(1600); }
void drawHome(){ frame("THE ASTRAL CABINET"); center("Welcome, "+guestName,54,1,MUTED); center("What would you like to consult?",72,1,CREAM); button(25,92,130,38,"DAILY OMEN"); button(165,92,130,38,"THREE CARDS"); button(25,143,130,38,"ZODIAC"); button(165,143,130,38,"CABINET"); center("Touch a doorway to begin",202,1,MUTED); }
void drawZodiac(){ frame("CHOOSE YOUR SIGN"); center("Your constellation",51,1,MUTED); for(int i=0;i<12;i++){ int col=i%4,row=i/4; int x=17+col*75,y=66+row*42; button(x,y,68,32,signs[i].name, i==signIndex?BURG:NAVY); } text("ELEMENT",30,200,1,GOLD); text(signs[signIndex].element,95,200,1,CREAM); text(signs[signIndex].theme,175,200,1,MUTED); footer(); }
void drawMenu(){ frame("SELECT A READING"); center(String(signs[signIndex].name)+"  /  "+signs[signIndex].element,53,1,GOLD); button(30,75,260,34,"ONE-CARD DAILY OMEN"); button(30,120,260,34,"PAST / PRESENT / BECOMING"); button(30,165,125,30,"CHANGE SIGN",NAVY); button(165,165,125,30,"CABINET",NAVY); footer(); }
String moonPhase(){ time_t now=time(nullptr); if(now<100000) return "the moon keeps its own time"; long days=(now/86400L)-10957; int phase=(days%30+30)%30; if(phase<2) return "new moon"; if(phase<7) return "waxing crescent"; if(phase<9) return "first quarter"; if(phase<14) return "waxing gibbous"; if(phase<16) return "full moon"; if(phase<22) return "waning gibbous"; if(phase<24) return "last quarter"; return "waning crescent"; }
void drawDaily(){ frame("DAILY OMEN"); const Card&c=cards[drawn[0]]; center(String(signs[signIndex].name)+"  ·  "+moonPhase(),51,1,GOLD); if(reveal==0){ button(70,72,180,92,"TOUCH TO DRAW"); center("Let the question arrive",182,1,MUTED); } else { center(c.name,70,2,CREAM); center(reversed[0]?"REVERSED":"UPRIGHT",94,1,reversed[0]?BURG:GOLD); textWrap(reversed[0]?c.rev:c.up,28,120,1,CREAM,265); center("Touch for another omen",202,1,MUTED); } footer(); }
void textWrap(const String&s,int x,int y,int size,uint16_t color,int width){ tft.setTextSize(size); String word,line; int yy=y; for(unsigned i=0;i<s.length();i++){ char ch=s[i]; if(ch==' '){ if(tft.textWidth(line+word)>width){ text(line,x,yy,size,color); yy+=12; line="";} line+=word+" "; word="";} else word+=ch; } line+=word; if(line.length()) text(line,x,yy,size,color); }
void drawSpread(){ frame("THREE CARD READING"); center(String(signs[signIndex].name)+"  ·  "+(reveal<3?"reveal the cards":"your constellation of meaning"),51,1,GOLD); const char* pos[]={"PAST","PRESENT","BECOMING"}; for(int i=0;i<3;i++){ int x=18+i*101; if(i>=reveal){ button(x,78,84,105,"REVEAL",BURG); text(pos[i],x+20,190,1,GOLD); } else { tft.fillRoundRect(x,78,84,105,6,NAVY); tft.drawRoundRect(x,78,84,105,6,GOLD); text(cards[drawn[i]].name,x+7,88,1,CREAM); text(reversed[i]?"REV":"UP",x+27,107,1,reversed[i]?BURG:GOLD); textWrap(cards[drawn[i]].key,x+8,130,1,MUTED,68); text(pos[i],x+20,190,1,GOLD); } } if(reveal==3){ textWrap(cards[drawn[0]].up,25,207,1,MUTED,270); } footer(); }
void drawCabinet(){ frame("THE CABINET"); center("A little archive of the self",50,1,MUTED); text("GUEST",28,76,1,GOLD); text(guestName,110,76,2,CREAM); button(28,98,82,25,"GUEST"); button(119,98,82,25,"MOON CHILD",NAVY); button(210,98,82,25,"STAR SEEKER",NAVY); text("SIGN",28,136,1,GOLD); text(signs[signIndex].name,110,136,2,CREAM); text("ELEMENT",28,164,1,GOLD); text(signs[signIndex].element,110,164,1,CREAM); text("MOON",28,184,1,GOLD); text(moonPhase(),110,184,1,CREAM); button(70,207,180,20,"RETURN TO RITUAL",BURG); footer(); }
void newReading(bool three){ reveal=0; for(int i=0;i<3;i++){ drawn[i]=random(22); reversed[i]=random(100)<25; } screen=three?SPREAD:DAILY; }
void tap(int x,int y){ if(screen==HOME){ if(y>88&&y<135){ if(x<160)newReading(false); else newReading(true); } else if(y>140&&y<187){ if(x<160)screen=ZODIAC; else screen=CABINET; } }
 else if(screen==ZODIAC){ if(y>=62&&y<195){ int col=(x-17)/75,row=(y-66)/42; if(col>=0&&col<4&&row>=0&&row<3) signIndex=row*4+col; } if(y>195) screen=MENU; }
 else if(screen==MENU){ if(y>70&&y<115)newReading(false); else if(y>115&&y<160)newReading(true); else if(y>160&&x<160)screen=ZODIAC; else if(y>160)screen=CABINET; }
 else if(screen==DAILY){ if(x<45&&y<45)screen=HOME; else { reveal=1; if(y>180)newReading(false); } }
 else if(screen==SPREAD){ if(x<45&&y<45)screen=HOME; else if(reveal<3)reveal++; else if(y>195)newReading(true); }
 else if(screen==CABINET){ if(y>205)screen=HOME; else if(y>95&&y<130){ if(x<112)guestName="Astral Guest"; else if(x<207)guestName="Moon Child"; else guestName="Star Seeker"; prefs.putString("name",guestName); } else if(x<45&&y<45)screen=HOME; }
}
void setup(){ Serial.begin(115200); randomSeed(analogRead(34)); tft.init(); tft.setRotation(1); touch.begin(); touch.setRotation(1); prefs.begin("cabinet",false); guestName=prefs.getString("name",DEFAULT_NAME); splash(); drawHome(); }
void loop(){ if(touch.touched()){ TS_Point p=touch.getPoint(); int x=map(p.x,TOUCH_X_MIN,TOUCH_X_MAX,0,W); int y=map(p.y,TOUCH_Y_MIN,TOUCH_Y_MAX,0,H); x=constrain(x,0,W-1); y=constrain(y,0,H-1); tap(x,y); while(touch.touched())delay(10); delay(120); } static Screen last=HOME; static int lastReveal=-1; if(last!=screen||lastReveal!=reveal){ if(screen==HOME)drawHome(); else if(screen==ZODIAC)drawZodiac(); else if(screen==MENU)drawMenu(); else if(screen==DAILY)drawDaily(); else if(screen==SPREAD)drawSpread(); else if(screen==CABINET)drawCabinet(); last=screen; lastReveal=reveal; } }
