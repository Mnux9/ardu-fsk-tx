#include <LiquidCrystal_I2C.h>
#include "si5351.h"
#include "Wire.h"
#include <FreqMeasure.h>

Si5351 si5351;
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows

//define IO pins
#define TXLED 13
#define RXLED 5
#define REL 6

//Transmit/receive status (0=RX, 1=TX)
int trStatus = 0;

//frequencies
int long wsFreq = 7000000;
int long txFreq = 7001500;
int long freqMesure = 1500;

int32_t cal_factor = 132300;

//Volt meter
int Vmesure = 155;
float Voltage = 12.00;

int long test = 14000000;

//Serial stuff
static String buffer;

//Default frequency in Hz (doesnt really matter JWST-X will change it anyways)
String freq = "00007000000";

//human frequency
String freqMHz = "07" ;
String freqHz = "00000" ;

//freq in Hz

//freq mesure stuff
double sum=0;
int count=0;

float frequency;

//setup
void setup() {
  lcd.init(); // initialize the lcd
  lcd.backlight();

  Serial.begin(9600);
  Serial.print(" ");

  //by default the counter pin is D8
  FreqMeasure.begin();

  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  //si5351.set_correction(cal_factor, SI5351_PLL_INPUT_XO);
  
  pinMode(TXLED, OUTPUT);
  pinMode(RXLED, OUTPUT);
  pinMode(REL, OUTPUT);

  digitalWrite(TXLED, LOW);
  digitalWrite(RXLED, HIGH);
  digitalWrite(REL, HIGH);  

  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("READY!");
  lcd.setCursor(0, 1);
  lcd.print("Conn CAT");

}


//loop
void loop() {  
    while (Serial.available() > 0) {
        char rx = Serial.read();
        buffer += rx;
        if (rx != ';') { // THIS IS IMPORTANT!!! -M
            continue;
        }
        buffer.trim();
        
        //This is where the arduino anwsers the requests sent from WSJT-X
        //Static anwsers that dont change in my usecase (see kenwood ts-480 cat datasheet for more info)
        //Radio ID (TS-480) 
        if (buffer.equals("ID;")) {
            Serial.print("ID020;");
        }
        //Power status (Power ON)
        else if (buffer.startsWith("PS")) {
            Serial.print("PS1;");
            lcd.clear();
            lcd.print(freqMHz);
            lcd.print(",");
            lcd.print(freqHz);
//            lcd.setCursor(0,1);
//            lcd.print("MHz");
            lcd.setCursor(6,1);
            lcd.print("RX");
            //set receiver freq
            si5351.set_freq(wsFreq*100ULL, SI5351_CLK1);
            //Disable si5351 output n0 and enable n1
            si5351.output_enable(SI5351_CLK0, 0);
            si5351.output_enable(SI5351_CLK1, 1);
        }
        //Automatic status reporting (Disabled)
        else if (buffer.equals("AI;")) {
            Serial.print("AI0;");
        }
        //VFO B frequency
        else if (buffer.equals("FB;")) {
            Serial.print("FB00007000000;");
        }
        //Operating mode (USB)
        else if (buffer.equals("MD;")) {
            Serial.print("MD2;");
        }
        //DSP filtering bandwith (0)
        else if (buffer.equals("FW;")) {
            Serial.print("FW0000;");
        }

        //Dynamic anwsers that get changed
        //Radio status
        else if (buffer.equals("IF;")) {
            Serial.print("IF");
            Serial.print(freq);
            Serial.print("     +00000000002000000 ;");
            Vmesure = analogRead(A0);
            Voltage = Vmesure * 0.0645;
            lcd.setCursor(0,1);
            lcd.print(Voltage);
            lcd.setCursor(4,1);
            lcd.print("V");
            
        }
        //VFO A frequency
        else if (buffer.equals("FA;")) {
            Serial.print("FA");
            Serial.print(freq);
            Serial.print(";");
            //set receiver freq
            si5351.set_freq(wsFreq*100ULL, SI5351_CLK1);

        }

    
        //This is where the arduino acknowledges commands frm WSJT-X    
        //WSJT-X wants to change the VFO A freq
        else if (buffer.startsWith("FA0")) {
            freq = buffer.substring(2,13);
            freqMHz = buffer.substring(5,7);
            freqHz = buffer.substring(7,12);
            wsFreq = freq.toInt();
            si5351.set_freq(wsFreq*100ULL, SI5351_CLK1);
            lcd.setCursor(0,0);
            lcd.print(freqMHz);
            lcd.print(",");
            lcd.print(freqHz);
        }


        //TEST STUFF
        else if (buffer.startsWith("READ;")) {
            Serial.print(wsFreq);
        }


        //Transmitt
        else if (buffer.equals("TX;")) {
            digitalWrite(RXLED, LOW);
            digitalWrite(TXLED, HIGH);
            digitalWrite(REL, LOW);
            
            //Enable si5351 output n0 and disable n1
            si5351.output_enable(SI5351_CLK0, 1);
            si5351.output_enable(SI5351_CLK1, 0);
            trStatus = 1;            
            lcd.setCursor(6,1);
            lcd.print("TX");
        }
        //Receive
        else if (buffer.equals("RX;")) {
            digitalWrite(TXLED, LOW);
            digitalWrite(RXLED, HIGH);
            digitalWrite(REL, HIGH);
            //Disable si5351 output n0 and enable n1
            si5351.output_enable(SI5351_CLK0, 0);
            si5351.output_enable(SI5351_CLK1, 0);
            trStatus = 0; 
            lcd.setCursor(6,1);
            lcd.print("RX");
        }

        buffer = "";
        
    }




    //Tx
    //If tx=1 mesure audio frequency, add it to the base frequency (wsFreq) and set it as the si5351 freq    
    if (trStatus == 1 && FreqMeasure.available()) {
      // average several reading together
      sum = sum + FreqMeasure.read();
      count = count + 1;
      if (count > 30) {
        frequency = FreqMeasure.countToFrequency(sum / count);
        freqMesure = frequency;
        txFreq = freqMesure + wsFreq;
        //Serial.println(txFreq);
        si5351.set_freq(txFreq*100ULL, SI5351_CLK0);
        sum = 0;
        count = 0;
      }
    }
    
}
