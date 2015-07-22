// This #include statement was automatically added by the Spark IDE.
#include "ac-ir-remote/ac-ir-remote.h"

#define LED_PIN D1
#define IR_PIN D0
#define TMP36_PIN A0
#define VCC 3321
#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
#define PUBLISH_MILLIS (15 * 60 * 1000)
#define COMMAND_SIGNAL_MILLIS 20000

const int COMMAND_LENGTH = 27;    

unsigned char daikin[COMMAND_LENGTH]     = { 
0x11,0xDA,0x27,0xF0,0x00,0x00,0x00,0x20,
//0    1    2   3    4    5     6   7
0x11,0xDA,0x27,0x00,0x00,0x41,0x1E,0x00,
//8    9   10   11   12    13   14   15
0xB0,0x00,0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,0xE3 };
//16  17    18  19   20    21   22  23   24   25   26

/*
byte 13=mode
b7 = 0
b6+b5+b4 = Mode
b3 = 0
b2 = OFF timer set
b1 = ON timer set
b0 = Air Conditioner ON
Modes: b6+b5+b4
011 = Cool
100 = Heat (temp 23)
110 = FAN (temp not shown, but 25)
000 = Fully Automatic (temp 25)
010 = DRY (temp 0xc0 = 96 degrees c)
byte 14=temp*2
byte 16=Fan
FAN control
b7+b6+b5+b4 = Fan speed
b3+b2+b1+b0 = Swing control up/down
Fan: b7+b6+b5+b4
030 = 1 bar - 48
040 = 2 bar - 64
050 = 3 bar - 80
060 = 4 bar - 96
070 = 5 bar - 112
0xa0 = Auto - 160
0xb0 = Not auto, moon + tree - 176
Swing control up/down:
0000 = Swing up/down off
1111 = Swing up/down on
Swing control left/right:
0000 = Swing left/right off
1111 = Swing left/right on
*/

IRsend irsend(IR_PIN);
 
#define IRSTATE_EEPROM_ADDR ((byte*) 0x100)
int incomingByte; 
struct IRState {
    byte mode;
    byte temp;
    byte fan;
    byte aux;
    byte state;
    byte enabled;
    byte sched;
    byte hour;
    byte minutes;
    long lastused;
} irstate;

volatile boolean commandReceived = false;
double centigrade = 0.0;
char temp[20];
unsigned long lastSync = millis();
unsigned long lastPublish = millis();
unsigned long lastCommand;

void setup()
{
    // WiFi.antennaSelect(ANT_INTERNAL);
    pinMode(LED_PIN, OUTPUT);
    pinMode(TMP36_PIN, INPUT);
    Spark.function("daikin", control);
    Spark.variable("temperature", &centigrade, DOUBLE);
}

void loop() {
    if (commandReceived) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(250);
        if (millis() - lastCommand > COMMAND_SIGNAL_MILLIS) {
            commandReceived = false;
        }
    } else {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(2000);
    }
    centigrade = getCentigrade();
    
    if (millis() - lastSync > ONE_DAY_MILLIS) {
        // Request time synchronization from the Particle Cloud
        // Spark.syncTime();
        lastSync = millis();
    }
    
    if (millis() - lastPublish > PUBLISH_MILLIS) {
        sprintf(temp, "%d", (int) (centigrade + 0.5));
        Spark.publish("temperature", temp, 60, PRIVATE);
        lastPublish = millis();
    }
} 
 
int control(String args)
{
    /*int rawSize = sizeof(rawCodes)/sizeof(int); // In this example, rawSize would evaluate to 37
    irsend.sendRaw(rawCodes, rawSize, 38);
    return 1;
    */
             
/*                  
Fan: b7+b6+b5+b4
030 = 1 bar - 48
040 = 2 bar - 64
050 = 3 bar - 80
060 = 4 bar - 96
070 = 5 bar - 112
0xa0 = 0 Auto - 160*/


/*
Modes: b6+b5+b4
0   ------000 = Fully Automatic (temp 25)
2   ------ 010 = DRY (temp 0xc0 = 96 degrees c)
3   ------011 = Cool
4   ------100 = Heat (temp 23)
5   ------110 = FAN (temp not shown, but 25)

args = temp-fan-mode
*/
    lastCommand = millis();
    commandReceived = true;

    if (args=="off") {
        airController_off(); 
        irstate.aux=airController_getAux();
        irstate.temp=airConroller_getTemp();
        irstate.fan= airConroller_getFan();
        irstate.mode=airConroller_getMode();
        
        irsend.sendDaikin(daikin, 8,0); 
        delay(29);
        irsend.sendDaikin(daikin, 19,8); 
        
    } else {
        String temps = getValue(args, '-', 0);
        String fans = getValue(args, '-', 1);
        String modes = getValue(args, '-', 2);
        
        int temp = temps.toInt();
        int fan = 0;
        int fanc = fans.toInt();
        int mode = modes.toInt();
    
        switch (fanc) {
        case 1:
            fan = 48;
            break;
        case 2:
            fan = 64;
            break;
        case 3:
            fan = 80;
            break;
        case 4:
            fan = 96;
            break;
        case 5:
            fan = 112;
            break;
        case 0:
            fan = 160;
            break;
        default:;
            break; 
        }
    
        airController_on ();
        airController_setTemp (temp);
        airController_setFan (fan);
        airController_setMode (mode);
        airController_checksum ();
        irsend.sendDaikin (daikin, 8,0);
        delay (29);
        irsend.sendDaikin (daikin, 19,8);
    }

    return 1;
}

String getValue(String data, char separator, int index) {
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length()-1;
    for (int i=0; i<=maxIndex && found<=index; i++) {
        if (data.charAt(i)==separator || i==maxIndex) {
            found++;
            strIndex[0] = strIndex[1]+1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

uint8_t airController_checksum()
{
	uint8_t sum = 0;
	uint8_t i;

	for(i = 0; i <= 6; i++){
		sum += daikin[i];
	}

    daikin[7] = sum &0xFF;
        
    sum=0;
	for(i = 8; i <= 25; i++){
		sum += daikin[i];
    }

    daikin[26] = sum &0xFF;
}


void airController_on(){
	//state = ON;
	daikin[13] |= 0x01;
	airController_checksum();
}

void airController_off(){
	//state = OFF;
	daikin[13] &= 0xFE;
	airController_checksum();
}

void airController_setAux(uint8_t aux){
	daikin[21] = aux;
	airController_checksum();
}

uint8_t airController_getAux(){
	return daikin[21];
}


void airController_setTemp(uint8_t temp)
{
	daikin[14] = (temp)*2;
	airController_checksum();
}


void airController_setFan(uint8_t fan)
{
	daikin[16] = fan;
	airController_checksum();
}


uint8_t airConroller_getTemp()
{
	return (daikin[14])/2;
}


uint8_t airConroller_getMode()
{

/*
Modes: b6+b5+b4
3   ------011 = Cool
4   ------100 = Heat (temp 23)
5   ------110 = FAN (temp not shown, but 25)
0   ------000 = Fully Automatic (temp 25)
2   ------ 010 = DRY (temp 0xc0 = 96 degrees c)
*/

	return (daikin[13])>>4;

}


void airController_setMode(uint8_t mode)
{
	daikin[13]=mode<<4 | airConroller_getState();
	airController_checksum();
}


uint8_t airConroller_getState()
{
	return (daikin[13])&0x01;
}

uint8_t airConroller_getFan()
{
	return (daikin[16]);
}

void restartac () {
  
    if(airConroller_getState()==1) {
        airController_off(); 
        irstate.aux=airController_getAux();
        irstate.temp=airConroller_getTemp();
        irstate.fan= airConroller_getFan();
        irstate.mode=airConroller_getMode();
        
        irsend.sendDaikin(daikin, 8,0); 
        delay(29);
        irsend.sendDaikin(daikin, 19,8); 
        
        delay (10000);
        
        airController_on(); 
        airController_setAux(0); 
        airController_setTemp(irstate.temp);
        airController_setFan(irstate.fan);
        airController_setMode(irstate.mode);
        
        irsend.sendDaikin(daikin, 8,0); 
        delay(29);
        irsend.sendDaikin(daikin, 19,8); 
    }
}

double getCentigrade()
{
	// http://www.analog.com/media/en/technical-documentation/data-sheets/TMP35_36_37.pdf
	readMilliVolts(TMP36_PIN);
	short mV = readMilliVolts(TMP36_PIN);
	return (mV / 1000.0 - 0.5) * 100.0;
}

short readMilliVolts(int pin)
{
	return map(analogRead(pin), 0, 4095, 0, VCC);
}