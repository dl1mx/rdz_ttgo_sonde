#include <U8x8lib.h>
#include <U8g2lib.h>
#include <SPIFFS.h>

#include <MicroNMEA.h>
#include "Display.h"
#include "Sonde.h"

extern Sonde sonde;

extern MicroNMEA nmea;
extern U8X8_SSD1306_128X64_NONAME_SW_I2C *u8x8;

const char *sondeTypeStr[5] = { "DFM6", "DFM9", "RS41", "RS92" };

byte myIP_tiles[8*11];
static uint8_t ap_tile[8]={0x00,0x04,0x22,0x92, 0x92, 0x22, 0x04, 0x00};

static const uint8_t font[10][5]={
  0x3E, 0x51, 0x49, 0x45, 0x3E,   // 0
  0x00, 0x42, 0x7F, 0x40, 0x00,   // 1
  0x42, 0x61, 0x51, 0x49, 0x46,   // 2
  0x21, 0x41, 0x45, 0x4B, 0x31,   // 3
  0x18, 0x14, 0x12, 0x7F, 0x10,   // 4
  0x27, 0x45, 0x45, 0x45, 0x39,   // 5
  0x3C, 0x4A, 0x49, 0x49, 0x30,   // 6
  0x01, 0x01, 0x79, 0x05, 0x03,   // 7
  0x36, 0x49, 0x49, 0x49, 0x36,   // 8
  0x06, 0x49, 0x39, 0x29, 0x1E }; // 9;  .=0x40


static unsigned char kmh_tiles[] U8X8_PROGMEM = {
   0x1F, 0x04, 0x0A, 0x11, 0x00, 0x1F, 0x02, 0x04, 0x42, 0x3F, 0x10, 0x08, 0xFC, 0x22, 0x20, 0xF8
   };
static unsigned char ms_tiles[] U8X8_PROGMEM = {
   0x1F, 0x02, 0x04, 0x02, 0x1F, 0x40, 0x20, 0x10, 0x08, 0x04, 0x12, 0xA8, 0xA8, 0xA4, 0x40, 0x00
   };
static unsigned char stattiles[5][4] =  {
   0x00, 0x1F, 0x00, 0x00 ,   // | == ok
   0x00, 0x10, 0x10, 0x00 ,   // . == no header found
   0x1F, 0x15, 0x15, 0x00 ,   // E == decode error
   0x00, 0x00, 0x00, 0x00 ,   // ' ' == unknown/unassigned
   0x07, 0x05, 0x07, 0x00 };  // ° = rx ok, but no valid position

static unsigned char arrowtiles[8][8] =  {
   0x00, 0x10, 0x20, 0x40 , 0xFF, 0x40, 0x20, 0x10 ,   // South
   0xF8, 0xC0, 0xA0, 0x90 , 0x88, 0x04, 0x01, 0x01 ,   // South-West
   0x08, 0x1C, 0x2A, 0x49 , 0x08, 0x08, 0x08, 0x08 ,   // West
   0x1F, 0x03, 0x05, 0x09 , 0x11, 0x20, 0x40, 0x80 ,   // North-West
   0x00, 0x08, 0x04, 0x02 , 0xFF, 0x02, 0x04, 0x08 ,   // North
   0x80, 0x40, 0x20, 0x11 , 0x09, 0x05, 0x03, 0x1F ,   // North-East
   0x08, 0x08, 0x08, 0x08 , 0x49, 0x2A, 0x1C, 0x08 ,   // East
   0x01, 0x02, 0x04, 0x88 , 0x90, 0xA0, 0xC0, 0xF8 };  // South-East

//static uint8_t halfdb_tile[8]={0x80, 0x27, 0x45, 0x45, 0x45, 0x39, 0x00, 0x00};

static uint8_t halfdb_tile1[8]={0x00, 0x38, 0x28, 0x28, 0x28, 0xC8, 0x00, 0x00};
static uint8_t halfdb_tile2[8]={0x00, 0x11, 0x02, 0x02, 0x02, 0x01, 0x00, 0x00};

//static uint8_t empty_tile[8]={0x80, 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x00};

static uint8_t empty_tile1[8]={0x00, 0xF0, 0x88, 0x48, 0x28, 0xF0, 0x00, 0x00};
static uint8_t empty_tile2[8]={0x00, 0x11, 0x02, 0x02, 0x02, 0x01, 0x00, 0x00};

//static uint8_t gps_tile[8]={0x3E, 0x77, 0x63, 0x77, 0x3E, 0x1C, 0x08, 0x00};
static uint8_t gps_tile[8]={0x00, 0x0E, 0x1F, 0x3B, 0x71, 0x3B, 0x1F, 0x0E};
static uint8_t nogps_tile[8]={0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x00};

static uint8_t deg_tile[8]={0x00, 0x06,0x09, 0x09, 0x06, 0x00, 0x00, 0x00};

#define SETFONT(large) u8x8->setFont((large)?u8x8_font_7x14_1x2_r:u8x8_font_chroma48medium8_r);

/* Description of display layouts.
 * for each display, the content is described by a DispEntry structure
 * timeout values are in milliseconds, for view activ, rx signal present, no rx signal present
 * for each displey, actions (switching to different sonde or different view) can be defined
 * based on key presses or on expired timeouts
 */
DispEntry searchLayout[] = {
	{0, 0, FONT_LARGE, disp.drawText, "Scan:"},
	{0, 8, FONT_LARGE, disp.drawType, NULL},
	{3, 0, FONT_LARGE, disp.drawFreq, " MHz"},
	{5, 0, FONT_LARGE, disp.drawSite, NULL},
	{7, 5, 0, disp.drawIP, NULL},	
	{-1, -1, -1, NULL, NULL},
};
int16_t searchTimeouts[] = { -1, 0, 0 };
uint8_t searchActions[] = {
	ACT_NONE,
	ACT_DISPLAY_DEFAULT, ACT_NONE, ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_NONE, ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_DISPLAY_DEFAULT, ACT_NEXTSONDE};
DispEntry legacyLayout[] = {
	{0, 5, FONT_SMALL, disp.drawFreq, " MHz"},
	{1, 8, FONT_SMALL, disp.drawAFC, NULL},
	{0, 0, FONT_SMALL, disp.drawType, NULL},
	{1, 0, FONT_SMALL, disp.drawID, NULL},
	{2, 0, FONT_LARGE, disp.drawLat, NULL},
	{4, 0, FONT_LARGE, disp.drawLon, NULL},
	{2, 10, FONT_SMALL, disp.drawAlt, NULL},
	{3, 10, FONT_SMALL, disp.drawHS, NULL},
	{4, 9, FONT_SMALL, disp.drawVS, NULL},
	{6, 0, FONT_LARGE, disp.drawRSSI, NULL},
	{6, 7, 0, disp.drawQS, NULL},
	{7, 5, 0, disp.drawIP, NULL},	
	{-1, -1, -1, NULL, NULL},
};
int16_t legacyTimeouts[] = { -1, -1, 20000 };
uint8_t legacyActions[] = {
	ACT_NONE,
	ACT_NEXTSONDE, ACT_DISPLAY(0), ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_DISPLAY(2), ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_NONE, ACT_DISPLAY(0)};
DispEntry fieldLayout[] = {
	{2, 0, FONT_LARGE, disp.drawLat, NULL},
	{4, 0, FONT_LARGE, disp.drawLon, NULL},
	{3, 10, FONT_SMALL, disp.drawHS, NULL},
	{4, 9, FONT_SMALL, disp.drawVS, NULL},
	{0, 0, FONT_LARGE, disp.drawID, NULL},
	{6, 0, FONT_LARGE, disp.drawAlt, NULL},
	{6, 7, 0, disp.drawQS, NULL},
	{-1, -1, -1, NULL, NULL},
};
int16_t fieldTimeouts[] = { -1, -1, -1 };
uint8_t fieldActions[] = {
	ACT_NONE,
	ACT_NEXTSONDE, ACT_DISPLAY(0), ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_DISPLAY(4), ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_NONE, ACT_NONE};
DispEntry field2Layout[] = {
	{2, 0, FONT_LARGE, disp.drawLat, NULL},
	{4, 0, FONT_LARGE, disp.drawLon, NULL},
	{1, 12, FONT_SMALL, disp.drawType, NULL},
	{0, 9, FONT_SMALL, disp.drawFreq, ""},
	{3, 10, FONT_SMALL, disp.drawHS, NULL},
	{4, 9, FONT_SMALL, disp.drawVS, NULL},
	{0, 0, FONT_LARGE, disp.drawID, NULL},
	{6, 0, FONT_LARGE, disp.drawAlt, NULL},
	{6, 7, 0, disp.drawQS, NULL},
	{-1, -1, -1, NULL, NULL},
};
uint8_t field2Actions[] = {
	ACT_NONE,
	ACT_NEXTSONDE, ACT_DISPLAY(0), ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_DISPLAY(1), ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_NONE, ACT_NONE};
DispEntry gpsLayout[] = {
	{0, 0, FONT_LARGE, disp.drawID, NULL},
	{2, 0, FONT_SMALL, disp.drawLat, NULL},
	{3, 0, FONT_SMALL, disp.drawLon, NULL},
	{4, 0, FONT_SMALL, disp.drawAlt, NULL},
	{6, 0, FONT_SMALL, disp.drawGPS, "V"},
	//{6, 1, FONT_SMALL, disp.drawGPS, "A"},
	//{6, 8, FONT_SMALL, disp.drawGPS, "O"},
	{7, 0, FONT_SMALL, disp.drawGPS, "D"},
	{7, 8, FONT_SMALL, disp.drawGPS, "I"},
	{-1, -1, -1, NULL, NULL},
};
uint8_t gpsActions[] = {
	ACT_NONE,
	ACT_NEXTSONDE, ACT_DISPLAY(0), ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_DISPLAY(1), ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_NONE, ACT_NONE};

DispInfo staticLayouts[5] = {
  { searchLayout, searchActions, searchTimeouts },
  { legacyLayout, legacyActions, legacyTimeouts },
  { fieldLayout, fieldActions, fieldTimeouts },
  { field2Layout, field2Actions, fieldTimeouts },
  { gpsLayout, gpsActions, fieldTimeouts } };

DispInfo *layouts = staticLayouts;

char Display::buf[17];

Display::Display() {
	setLayout(0);
}

#define MAXSCREENS 10
#define DISP_ACTIONS_N 12
#define DISP_TIMEOUTS_N 3

void Display::freeLayouts() {
	if(layouts==staticLayouts) return;
	DispInfo *old = layouts;
	layouts=staticLayouts;
	setLayout(0);
	for(int i=0; i<MAXSCREENS; i++) {
		if(old[i].de) free(old[i].de);
	}
	free(old);
}

int Display::allocDispInfo(int entries, DispInfo *d)
{
	int totalsize = (entries+1)*sizeof(DispEntry) + DISP_ACTIONS_N*sizeof(uint8_t) + DISP_TIMEOUTS_N * sizeof(int16_t);
	char *mem = (char *)malloc(totalsize);
	if(!mem) return -1;
	memset (mem, 0, totalsize);

	d->de = (DispEntry *)mem;
	mem += (entries+1) * sizeof(DispEntry);

	d->actions = (uint8_t *)mem;
	mem += DISP_ACTIONS_N * sizeof(uint8_t);
	d->actions[0] = ACT_NONE;

	d->timeouts = (int16_t *)mem;
	Serial.printf("allocated %d bytes (%d entries) for %p (addr=%p)\n", totalsize, entries, d, d->de);
	return 0;
}

void Display::parseDispElement(char *text, DispEntry *de)
{
	char type = *text;
	if(type>='A'&&type<='Z') {
		type += 32;  // lc
		de->fmt = FONT_LARGE;
	} else {
		de->fmt = FONT_SMALL;
	}
	switch(type) {
	case 'l':
		de->func = disp.drawLat; break;
	case 'o':
		de->func = disp.drawLon; break;
	case 'a':
		de->func = disp.drawAlt; break;
	case 'h':
		de->func = disp.drawHS; break;
	case 'v':
		de->func = disp.drawVS; break;
	case 'i':
		de->func = disp.drawID;
		de->extra = strdup(text+1);
		break;
	case 'q':
		de->func = disp.drawQS; break;
	case 't':
		de->func = disp.drawType; break;
	case 'c':
		de->func = disp.drawAFC; break;
	case 'f':
		de->func = disp.drawFreq;
		de->extra = strdup(text+1);
		Serial.printf("parsing 'f' entry: extra is '%s'\n", de->extra);
		break;
	case 'n':
		de->func = disp.drawIP; break;
	case 's':
		de->func = disp.drawSite; break;
	case 'g':
		de->func = disp.drawGPS;
		de->extra = strdup(text+1);
		Serial.printf("parsing 'g' entry: extra is '%s'\n", de->extra);
		break;
	case 'r':
		de->func = disp.drawRSSI; break;
	case 'x':
		de->func = disp.drawText;
		de->extra = strdup(text+1);
		break;
	default:
		Serial.printf("Unknown element: %c\n", type);
		break;
	}
}

static uint8_t ACTION(char c) {
	switch(c) {
	case 'D': 
		return ACT_DISPLAY_DEFAULT;
	case 'F':
		return ACT_DISPLAY_SPECTRUM;
	case 'W':
		return ACT_DISPLAY_WIFI;
	case '+':
		return ACT_NEXTSONDE;
	case '#':	
		return ACT_NONE;
	default:
		if(c>='0'&&c<='9')
		return ACT_DISPLAY(c-'0');
	}
	return ACT_NONE;
}

void Display::initFromFile() {
	File d = SPIFFS.open("/screens.txt", "r");
	if(!d) return;

	freeLayouts();
	layouts = (DispInfo *)malloc(MAXSCREENS * sizeof(DispInfo));
	if(!layouts) {
		layouts = staticLayouts;
		return;
	}
	memset(layouts, 0, MAXSCREENS * sizeof(DispInfo));

	int idx = -1;
	int what = -1;
	int entrysize;
	Serial.printf("Reading from /screens.txt. available=%d\n",d.available());
	while(d.available()) {
		String line = d.readStringUntil('\n');
		line.trim();
		const char *s = line.c_str();
		Serial.printf("Line: '%s'\n", s);
		if(*s == '#') continue;  // ignore comments
		switch(what) {
		case -1:	// wait for start of screen (@)
			{
			if(*s != '@') {
				Serial.printf("Illegal start of screen: %s\n", s);
				continue;
			}
			char *num = strchr(s, ':');
			if(!num) {
				Serial.println("Line missing size length indication");
				continue;
			}
			entrysize = atoi(num+1);
			Serial.printf("Reading entry with %d elements\n", entrysize);
			idx++;
			int res = allocDispInfo(entrysize, &layouts[idx]);
			if(res<0) {
				Serial.println("Error allocating memory for disp info");
				continue;
			}
			what = 0;
			}
			break;
		default:	// parse content... (additional data or line `what`)
			if(strncmp(s,"timer=",6)==0) {  // timer values
				sscanf(s+6, "%hd,%hd,%hd", layouts[idx].timeouts, layouts[idx].timeouts+1, layouts[idx].timeouts+2);
				Serial.printf("timer values: %d, %d, %d\n", layouts[idx].timeouts[0], layouts[idx].timeouts[1], layouts[idx].timeouts[2]);
			} else if(strncmp(s, "key1action=",11)==0) { // key 1 actions
				char c1,c2,c3,c4;
				sscanf(s+11, "%c,%c,%c,%c", &c1, &c2, &c3, &c4);
				layouts[idx].actions[1] = ACTION(c1);
				layouts[idx].actions[2] = ACTION(c2);
				layouts[idx].actions[3] = ACTION(c3);
				layouts[idx].actions[4] = ACTION(c4);
			} else if(strncmp(s, "key2action=",11)==0) { // key 2 actions
				char c1,c2,c3,c4;
				sscanf(s+11, "%c,%c,%c,%c", &c1, &c2, &c3, &c4);
				layouts[idx].actions[5] = ACTION(c1);
				layouts[idx].actions[6] = ACTION(c2);
				layouts[idx].actions[7] = ACTION(c3);
				layouts[idx].actions[8] = ACTION(c4);
				Serial.printf("parsing key2action: %c %c %c %c\n", c1, c2, c3, c4);
			} else if(strncmp(s, "timeaction=",11)==0) { // timer actions
				char c1,c2,c3;
				sscanf(s+11, "%c,%c,%c", &c1, &c2, &c3);
				layouts[idx].actions[9] = ACTION(c1);
				layouts[idx].actions[10] = ACTION(c2);
				layouts[idx].actions[11] = ACTION(c3);
			} else if(strchr(s, '=')) {  // one line with some data...
				int x,y;
				char text[30];
				sscanf(s, "%d,%d=%30[^\r\n]", &y, &x, text);
				layouts[idx].de[what].x = x;
				layouts[idx].de[what].y = y;
				parseDispElement(text, layouts[idx].de+what);
				what++;
				layouts[idx].de[what].func = NULL;
			} else {
				for(int i=0; i<12; i++) {
					Serial.printf("action %d: %d\n", i, (int)layouts[idx].actions[i]);
				}
 				what=-1;
			}
			break;
		}
	}
}

void Display::setLayout(int layoutIdx) {
	layout = &layouts[layoutIdx];
}

void Display::drawLat(DispEntry *de) {
	SETFONT(de->fmt);
	if(!sonde.si()->validPos) {
	   u8x8->drawString(de->x,de->y,"<?""?>      ");
	   return;
	}
	snprintf(buf, 16, "%2.5f", sonde.si()->lat);
	u8x8->drawString(de->x,de->y,buf);
}
void Display::drawLon(DispEntry *de) {
	SETFONT(de->fmt);
	if(!sonde.si()->validPos) {
	   u8x8->drawString(de->x,de->y,"<?""?>      ");
	   return;
	}
	snprintf(buf, 16, "%2.5f", sonde.si()->lon);
	u8x8->drawString(de->x,de->y,buf);
}
void Display::drawAlt(DispEntry *de) {
	SETFONT(de->fmt);
	if(!sonde.si()->validPos) {
	   u8x8->drawString(de->x,de->y,"     ");
	   return;
	}
	snprintf(buf, 16, sonde.si()->alt>=1000?"   %5.0fm":"   %3.1fm", sonde.si()->alt);
	u8x8->drawString(de->x,de->y,buf+strlen(buf)-6);
}
void Display::drawHS(DispEntry *de) {
	SETFONT(de->fmt);
	if(!sonde.si()->validPos) {
	   u8x8->drawString(de->x,de->y,"     ");
	   return;
	}
	snprintf(buf, 16, sonde.si()->hs>99?" %3.0f":" %2.1f", sonde.si()->hs);
	u8x8->drawString(de->x,de->y,buf+strlen(buf)-4);
	u8x8->drawTile(de->x+4,de->y,2,kmh_tiles);
}
void Display::drawVS(DispEntry *de) {
	SETFONT(de->fmt);
	if(!sonde.si()->validPos) {
	   u8x8->drawString(de->x,de->y,"     ");
	   return;
	}
	snprintf(buf, 16, "  %+2.1f", sonde.si()->vs);
	u8x8->drawString(de->x, de->y, buf+strlen(buf)-5);
	u8x8->drawTile(de->x+5,de->y,2,ms_tiles);
}
void Display::drawID(DispEntry *de) {
	SETFONT((de->fmt&0x01));
	if(!sonde.si()->validID) {
		u8x8->drawString(de->x, de->y, "nnnnnnnn ");
		return;
	}
	// TODO: handle DFM6 IDs

	if(!de->extra || de->extra[0]=='s') {
		// real serial number, as printed on sonde
		u8x8->drawString(de->x, de->y, sonde.si()->id);
	} else if (de->extra[0]=='a') {
		// autorx sonde number ("DF9" and last 6 digits of real serial number
		strcpy(buf, sonde.si()->id);
		memcpy(buf, "DF9", 3);
		u8x8->drawString(de->x, de->y, buf);
	} else {
		// dxlAPRS sonde number (DF6 (why??) and 5 last digits of serial number as hex number
		uint32_t id = atoi(sonde.si()->id);
		id = id&0xfffff;
		snprintf(buf, 16, "DF6%05X", id);
		u8x8->drawString(de->x, de->y, buf);
	}
}
void Display::drawRSSI(DispEntry *de) {
	SETFONT(de->fmt);
	snprintf(buf, 16, "-%d   ", sonde.si()->rssi/2);
	int len=strlen(buf)-3;
	Serial.printf("drawRSSI: %d %d %d (%d)[%d]\n", de->y, de->x, sonde.si()->rssi/2, sonde.currentSonde, len);
	buf[5]=0;
	u8x8->drawString(de->x,de->y,buf);
	u8x8->drawTile(de->x+len, de->y, 1, (sonde.si()->rssi&1)?halfdb_tile1:empty_tile1);
	u8x8->drawTile(de->x+len, de->y+1, 1, (sonde.si()->rssi&1)?halfdb_tile2:empty_tile2);
}
void Display::drawQS(DispEntry *de) {
	uint8_t *stat = sonde.si()->rxStat;
	for(int i=0; i<18; i+=2) {
	        uint8_t tile[8];
	        *(uint32_t *)(&tile[0]) = *(uint32_t *)(&(stattiles[stat[i]]));
	        *(uint32_t *)(&tile[4]) = *(uint32_t *)(&(stattiles[stat[i+1]]));
	        u8x8->drawTile(de->x+i/2, de->y, 1, tile);
	}
}
void Display::drawType(DispEntry *de) {
	SETFONT(de->fmt);
        u8x8->drawString(de->x, de->y, sondeTypeStr[sonde.si()->type]);
}
void Display::drawFreq(DispEntry *de) {
	SETFONT(de->fmt);
        snprintf(buf, 16, "%3.3f%s", sonde.si()->freq, de->extra?de->extra:"");
        u8x8->drawString(de->x, de->y, buf);
}
void Display::drawAFC(DispEntry *de) {
 	if(!sonde.config.showafc) return;
	SETFONT(de->fmt);
	if(sonde.si()->afc==0) { strcpy(buf, "        "); }
	else { snprintf(buf, 15, "     %+3.2fk", sonde.si()->afc*0.001); }
        u8x8->drawString(de->x, de->y, buf+strlen(buf)-8);
}
void Display::drawIP(DispEntry *de) {
        u8x8->drawTile(de->x, de->y, 11, myIP_tiles);

}
void Display::drawSite(DispEntry *de) {
        SETFONT(de->fmt);
	u8x8->drawString(de->x, de->y, sonde.si()->launchsite);
}
void Display::drawTelemetry(DispEntry *de) {
}

#define EARTH_RADIUS (6371000.0F)
#ifndef PI
#define  PI  (3.1415926535897932384626433832795)
#endif
// defined by Arduino.h   #define radians(x) ( (x)*180.0F/PI )

void Display::drawGPS(DispEntry *de) {
	if(sonde.config.gps_rxd<0) return;
	SETFONT(de->fmt);
	switch(de->extra[0]) {
	case 'V':
		{
		// show if GPS location is valid
		uint8_t *tile = nmea.isValid()?gps_tile:nogps_tile;
		u8x8->drawTile(de->x, de->y, 1, tile);
		}
		break;
	case 'O':
		// GPS long
		{
		float lon = nmea.getLongitude()*0.000001;
		Serial.print("lon: "); Serial.println(lon);
		snprintf(buf, 16, "%2.5f", lon);
		u8x8->drawString(de->x,de->y,buf);
		}
		break;
	case 'A':
		// GPS lat
		{
		float lat = nmea.getLatitude()*0.000001;
		Serial.print("lat: "); Serial.println(lat);
		snprintf(buf, 16, "%2.5f", lat);
		u8x8->drawString(de->x,de->y,buf);
		}
		break;
	case 'H':
		// GPS alt
		{
		long alt = -1;
		nmea.getAltitude(alt);
		snprintf(buf, 16, "%5fm", alt*0.00001);
		u8x8->drawString(de->x,de->y,buf);
		}
		break;
	case 'D':
		{
		// distance
		// equirectangular approximation is good enough
		if( (sonde.si()->validPos&0x03)!=0x03 ) {
			snprintf(buf, 16, "no pos ");
			if(de->extra && *de->extra=='5') buf[5]=0;
		} else if(!nmea.isValid()) {
			snprintf(buf, 16, "no gps ");
			if(de->extra && *de->extra=='5') buf[5]=0;
		} else {
			float lat1 = nmea.getLatitude()*0.000001;
			float lat2 = sonde.si()->lat;
			float x = radians(nmea.getLongitude()*0.000001-sonde.si()->lon) * cos( radians((lat1+lat2)/2) );
			float y = radians(lat2-lat1);
			float d = sqrt(x*x+y*y)*EARTH_RADIUS;
			if(de->extra && *de->extra=='5') { // 5-character version: ****m / ***km / **e6m
				if(d>999999) snprintf(buf, 16, "%de6m  ", (int)(d/1000000));
				if(d>9999) snprintf(buf, 16, "%dkm   ", (int)(d/1000));
				else snprintf(buf, 16, "%dm    ", (int)d);
				buf[5]=0;
			} else { // 6-character version: *****m / ****km)
				if(d>99999) snprintf(buf, 16, "%dkm    ", (int)(d/1000));
				else snprintf(buf, 16, "%dm     ", (int)d);
				buf[6]=0;
			}
		}
		u8x8->drawString(de->x, de->y, buf);
		}
		break;
	case 'I':
		// dIrection
		if( (!nmea.isValid()) || ((sonde.si()->validPos&0x03)!=0x03 ) ) {
			u8x8->drawString(de->x, de->y, "---");
			u8x8->drawString(de->x+3, de->y, " ");
			break;
		}
		{
		float lat1 = radians(nmea.getLatitude()*0.000001);
                float lat2 = radians(sonde.si()->lat);
		float lon1 = radians(nmea.getLongitude()*0.000001);
                float lon2 = radians(sonde.si()->lon);
		float y = sin(lon2-lon1)*cos(lat2);
		float x = cos(lat1)*sin(lat2) - sin(lat1)*cos(lat2)*cos(lon2-lon1);
		float dir = atan2(y, x)/PI*180;
		if(dir<0) dir+=360;
		Serial.printf("direction is %.2f\n", dir);
		snprintf(buf, 16, "%3d", (int)dir);
		buf[3]=0;
		u8x8->drawString(de->x, de->y, buf);
		if(de->extra[1]==(char)176)
			u8x8->drawTile(de->x+3, de->y, 1, deg_tile);
		// Show drive direction arrow
		float course = (nmea.getCourse()/1000.);
		Serial.printf("course is %.2f\n", course);
		float diff = (dir - course);
		if(diff>=180)
			diff = diff - 360;
		if(diff<-180)
			diff = diff + 360;
		Serial.printf("diff is %.2f\n", diff);
		int arrow = (int) (8 * (diff+180) / 360);
		Serial.printf("arrow is %d\n", arrow);
		uint8_t *tile = arrowtiles[arrow];
		u8x8->drawTile(de->x+3, de->y, 1, tile);
	}
		break;
	case 'E':
		// elevation
		break;
	}
}
void Display::drawText(DispEntry *de) {
        SETFONT(de->fmt);
	u8x8->drawString(de->x, de->y, de->extra);
}


void Display::clearIP() {
  memset(myIP_tiles, 0, 11*8);
}

void Display::setIP(const char *ip, bool AP) {
  memset(myIP_tiles, 0, 11*8);
  int len = strlen(ip);
  int pix = (len-3)*6+6;
  int tp = 80-pix+8;
  if(AP) memcpy(myIP_tiles+(tp<16?0:8), ap_tile, 8);
  for(int i=0; i<len; i++) {
        if(ip[i]=='.') { myIP_tiles[tp++]=0x40; myIP_tiles[tp++]=0x00; }
        else {
          int idx = ip[i]-'0';
          memcpy(myIP_tiles+tp, &font[idx], 5);
          myIP_tiles[tp+5] = 0;
          tp+=6;
        }
  }
  while(tp<8*10) { myIP_tiles[tp++]=0; }
}


void Display::updateDisplayPos() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawLat && di->func != disp.drawLon) continue;
		di->func(di);
	}
}
void Display::updateDisplayPos2() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawAlt && di->func != disp.drawHS && di->func != disp.drawVS) continue;
		di->func(di);
	}
}
void Display::updateDisplayID() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawID) continue;
		di->func(di);
	}
}
void Display::updateDisplayRSSI() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawRSSI) continue;
		di->func(di);
	}
}
void Display::updateStat() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawQS) continue;
		di->func(di);
	}
}

void Display::updateDisplayRXConfig() {
       for(DispEntry *di=layout->de; di->func != NULL; di++) {
                if(di->func != disp.drawQS && di->func != disp.drawAFC) continue;
                di->func(di);
        }
}
void Display::updateDisplayIP() {
       for(DispEntry *di=layout->de; di->func != NULL; di++) {
                if(di->func != disp.drawIP) continue;
		Serial.printf("updateDisplayIP: %d %d\n",di->x, di->y);
                di->func(di);
        }
}

void Display::updateDisplay() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		di->func(di);
	}
}

Display disp = Display();
