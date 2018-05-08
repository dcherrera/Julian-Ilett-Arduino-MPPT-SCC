// Wiring:
//A0 - Pot
//A1 - Volts (solar)
//A2 - Volts (battery)
//A4 - Amps (solar)
//A5 - Amps (battery)

//D3 - PWM out to DCOI
//D8 -LCD_RST (1)
//D9 - LCD_CE (2)
//D10 - LCD_DC (3)
//D11 - LCD_DIN (4)
//D12 - LCD_CLK (5)
//-----LCD_VCC (6)
//---LCD_LIGHT (7)
//-----LCD_GND (8)

#define LCD_C LOW
#define LCD_D HIGH

#define PIN_RESET 8
#define PIN_SCE 9
#define PIN_DC 10
#define PIN_SDIN 11
#define PIN_SCLK 12

//#include <stdlib.h>;
#include "header.h";
#include <PWM.h>
int32_t frequency = 15000; //frequency (in Hz)
int sensorValue = 0;
float panelVolts = 0;
float batteryVolts = 0;
float lastPanelVolts = 0;
float lastBatteryVolts = 0;
float panelAmps = 0;
float lastPanelAmps = 0;
float batteryAmps = 0;
float lastBatteryAmps = 0;
float efficiency = 0;
float siemens = 0;
float panelWatts = 0;
float lastPanelWatts = 0;
float batteryWatts = 0;
float maxwatts = 0;
int barwatts = 0;
float Voc = 0;
float Isc = 0;
char tmp[25];
//boolean gotVoc = false;
//boolean gotIsc = false;
boolean highWatts = false;
int chartX = 0;
int chartY = 0;
//int bin = 0;
boolean mpptOn = false;
const boolean initialise = true;

void LcdInitialise(void)
{
pinMode(PIN_SCE, OUTPUT);
pinMode(PIN_RESET, OUTPUT);
pinMode(PIN_DC, OUTPUT);
pinMode(PIN_SDIN, OUTPUT);
pinMode(PIN_SCLK, OUTPUT);
digitalWrite(PIN_RESET, LOW);
digitalWrite(PIN_RESET, HIGH);
LcdWrite(LCD_C, 0x21 ); // LCD Extended Commands.
LcdWrite(LCD_C, 0xBC ); // Set LCD Vop (Contrast).
LcdWrite(LCD_C, 0x04 ); // Set Temp coefficent. //0x04
LcdWrite(LCD_C, 0x14 ); // LCD bias mode 1:48. //0x13
LcdWrite(LCD_C, 0x0C ); // LCD in normal mode.
LcdWrite(LCD_C, 0x20 );
LcdWrite(LCD_C, 0x0C );
}

void LcdClear()
{
for (int i=0; i<504; i++) LcdWrite(LCD_D, 0x00);
}

void LcdWrite(byte dc, byte data)
{
digitalWrite(PIN_DC, dc);
digitalWrite(PIN_SCE, LOW);
shiftOut(PIN_SDIN, PIN_SCLK, MSBFIRST, data);
digitalWrite(PIN_SCE, HIGH);
}

void LcdString(char *characters, int x, int y)
{
LcdWrite(LCD_C, 0x80 | x); // Column.
LcdWrite(LCD_C, 0x40 | y); // Row.
while (*characters) LcdCharacter(*characters++);
}

void LcdCharacter(char character)
{
for (int index = 0; index < 5; index++) LcdWrite(LCD_D, ASCII[character - 0x20][index]);
LcdWrite(LCD_D, 0x00);
}

void LcdXY(int x, int y)
{
LcdWrite(LCD_C, 0x80 | x); // Column.
LcdWrite(LCD_C, 0x40 | y); // Row.
}

void LcdPlot (int x, int y)
{
// static int lastX;
// static int lastY;
LcdXY(x,5-(y/8)); //set display address for plot
int bin=128;
for (int q=0; q<y%8; q++) bin=bin/2; //calculate pixel position in byte
LcdWrite(LCD_D, bin); //plot pixel
float slope=float(47-y)/float(83-x);

for (int j=x+2; j<84; j++) {
float dy=slope*float(j-x);
int k=y+round(dy);
LcdXY(j,5-(k/8)); //set display address for plot
int bin=128;
for (int q=0; q<k%8; q++) bin=bin/2; //calculate pixel position in byte
LcdWrite(LCD_D, bin); //plot pixel
}
// lastX = x;
// lastY = y;
}

void perturb(boolean init=false)
{
static byte pulseWidth = 0;
static boolean trackDirection = false; //counts down / pwm increases
// static int loopCounter = 0;
if (init) {
pulseWidth = 255;
trackDirection = false;
}
else {
if (!trackDirection) {
if (pulseWidth != 0) {pulseWidth--;} else {trackDirection = true;}
}
else {
if (pulseWidth != 255) {pulseWidth++;} else {trackDirection = false;}
}
}
pwmWrite(3, pulseWidth); //write perturbed PWM value to PWM hardware
if ((panelWatts - lastPanelWatts) < -0.1) trackDirection = !trackDirection;
}

void setup()
{
LcdInitialise();
LcdClear();
LcdString("Solar",0,0);
LcdString("MuPPeT",0,1);
LcdString("%",71,1);
LcdString("V",31,2);
LcdString("V",71,2);
LcdString("A",31,3);
LcdString("A",71,3);
LcdString("W",26,4);
LcdString("Wp",65,4);

InitTimersSafe();
bool success = SetPinFrequencySafe(3, frequency);
if(success) {
pinMode(13, OUTPUT);
digitalWrite(13, HIGH);
}

}
void loop()
{
panelVolts = analogRead(A1) * 47.0 / 1023; //get the volts
panelVolts = (panelVolts + lastPanelVolts) / 2; //average the volts
lastPanelVolts = panelVolts;
LcdString(dtostrf(panelVolts,5,2,&tmp[0]),0,2); //display the volts

panelAmps = (514 - analogRead(A4)) * 27.03 / 1023; //get the panelAmps
panelAmps = (panelAmps + lastPanelAmps) / 2; //average the panelAmps
lastPanelAmps = panelAmps;
if (panelAmps < 0) panelAmps = 0; //don't let the panelAmps go below zero
LcdString(dtostrf(panelAmps,5,2,&tmp[1]),0,3); //display the panelAmps

panelWatts = panelVolts * panelAmps; //calculate the watts
LcdString(dtostrf(panelWatts,4,1,&tmp[2]),0,4); //display the panel watts

sensorValue = analogRead(A0);
if (sensorValue > 1020) {
// pwmWrite(3, sensorValue / 4);
if (mpptOn) {
perturb();
}
else {
mpptOn = true;
perturb(initialise); //initialise the perturb algorithm
LcdString("A",41,0); //display an A for auto
}
}
else {
if (!mpptOn) {
pwmWrite(3, sensorValue / 4);
}
else {
mpptOn = false;
LcdString("M",41,0); //display an M for manual
}
}

lastPanelWatts = panelWatts;

batteryVolts = analogRead(A2) * 25.0 / 1023; //get the battery volts
batteryVolts = (batteryVolts + lastBatteryVolts) / 2; //average the volts
lastBatteryVolts = batteryVolts;
LcdString(dtostrf(batteryVolts,5,2,&tmp[0]),40,2); //display the battery volts

batteryAmps = (514 - analogRead(A5)) * 27.03 / 1023; //get the panelAmps
batteryAmps = (batteryAmps + lastBatteryAmps) / 2; //average the panelAmps
lastBatteryAmps = batteryAmps;
if (batteryAmps < 0) batteryAmps = 0; //don't let the batteryAmps go below zero
LcdString(dtostrf(batteryAmps,5,2,&tmp[1]),40,3); //display the panelAmps

// if (volts > 1) {
// siemens = panelAmps / volts; //calculate the conductance
// LcdString(dtostrf(siemens,5,2,&tmp[2]),0,2); //display the siemens
// }

batteryWatts = batteryVolts * batteryAmps; //calculate the battery watts

efficiency = batteryWatts / panelWatts * 100;
LcdString(dtostrf(efficiency,3,0,&tmp[2]),50,1); //display the efficiency

maxwatts = max(maxwatts, panelWatts); //calculate the max watts
LcdString(dtostrf(maxwatts,4,1,&tmp[3]),40,4); //display the max watts

if (panelWatts > 27) highWatts = true; //go to 83Watt bargraph

LcdXY(0,5);
if (!highWatts) {
barwatts = round(panelWatts * 3);
for(int k=0; k<84; k++)
{
if (k >= barwatts) LcdWrite (LCD_D, 0x00);
else if (k % 30 == 0) LcdWrite (LCD_D, 0xf0);
else if (k % 15 == 0) LcdWrite (LCD_D, 0xe0);
else LcdWrite (LCD_D, 0xc0);
}
}
else {
barwatts = round(panelWatts);
for(int k=0; k<84; k++)
{
if (k >= barwatts) LcdWrite (LCD_D, 0x00);
else if (k % 10 == 0) LcdWrite (LCD_D, 0xf0);
else if (k % 5 == 0) LcdWrite (LCD_D, 0xe0);
else LcdWrite (LCD_D, 0xc0);
}
}

Voc = max(Voc, panelVolts);
Isc = max(Isc, panelAmps);

chartX = min(panelVolts/Voc*84, 83);
chartY = min(panelAmps/Isc*48, 47);

if (chartX > 41 & chartY > 40) { //if in upper right quadrant
LcdPlot (chartX, chartY);
}

delay(10);
}
