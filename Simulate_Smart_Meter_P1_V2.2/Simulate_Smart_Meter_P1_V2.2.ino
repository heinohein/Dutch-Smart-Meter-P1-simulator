//---------------------------------------------------------------------------------------------
// 3-1-2017 copied from https://forum.mysensors.org/topic/1978/power-usage-sensor-multi-channel-local-display/2
//
// Arduino Pulse Counting Sketch for counting pulses from up to 12 pulse output meters.
// uses direct port manipulation to read from each register of 6 digital inputs simultaneously
//
// Licence: GNU GPL
// part of the openenergymonitor.org project
//
// Original Author: Trystan Lea
// AWI: adapted to produce JSON and error checking at client side.
//
// 2-9-2018   Read port A1 to allow stop transmission, controlled by AWI Master (CTS). Therefore allows 
//            2 AWI slaves connected to one AWI Master
//            if <=12 energy meters used, leave unused NC. Arduino's pull-up resistors stop antenna effect
// 30-10-2018 Reset triggered by AWI Master to start a new day with 0. RST connected to port D9 of Master via 220 Ohm resistor.
//            Resistor needed otherwise upload scetch not possible
// 28-8-2019  V1.0 Special version for simulate Smart Meter P1 output for only 1 energy meter input
// 27-9-2019  V2.0 Multiple meters and simulate Smart Meter P1 output
// 27-9-2019  V2.1 Output is now in kWh and kW as requested by the P1 protokcoll 
//                 position meternr was 1.8.x must be x.8.1, corrected
// 30-9-2019  V2.2 For Test Random added Development stopped, Domoticz shows only meter 1 and 2
//---------------------------------------------------------------------------------------------
//PLEASE CONFIGURE lastMeter AND COMMENT OUT OR NOT (SEARCH BELOW FOR) COMPUTED TOTALS
//---------------------------------------------------------------------------------------------
class PulseOutput
{
public:                                      //AWI: access to all
  boolean pulse(int,int,unsigned long);                  //Detects pulses, in pulseLib.ino
  unsigned long rate( unsigned long );                   //Calculates rate 

  unsigned long count;                                   //pulse count accumulator
  unsigned long countAccum;                              //pulse count total accumulator for extended error checking (only resets at startup)
  unsigned long prate;                                   //pulse width in time 
  unsigned long prateAccum;                              //pulse rate accumulator for calculating mean.
  float dayUsagekWh;                                     //total usage today in Watthour
private:
  boolean ld,d;                                          //used to determine pulse edge
  unsigned long lastTime,time;                           //used to calculate rate
};

//---------------------------------------------------------------------------------------------
// Variable declaration
//---------------------------------------------------------------------------------------------

//CHANGE THIS TO VARY RATE AT WHICH PULSE COUNTING ARDUINO SPITS OUT PULSE COUNT+RATE DATA
//time in seconds;
const unsigned long printTime = 10000000;  // delay between serial outputs in micro seconds (one meter at a time)  
const byte lastMeter = 3 ;                 // is number of S0 meters + 1 (2->13)

byte curMeter = 2 ;                        // current meter for serial output, wraps from 2 to lastMeter
unsigned long c;                           // pulse count field
char data11[11];                           // workfiels for sprintf statement

//---------------------------------------------------------------------------------------------
PulseOutput p[14];                         //Pulse output objects

int a,b,la,lb;                             //Input register variables

unsigned long ltime, time, deltaTime;      //time variables

#define CTS A1                             // use port A1 for CTS clear to send to master 
unsigned long ctsTime=0ul;                 // time when a CTS low stopped transmitting to master
unsigned long deltaCtsTime=10000000ul;     // 10 secondes

float momentPowerkW;                       // workfields power computation
float totalMomentPowerkW;                  // running total of momentPowerkW
float contributePowerkW;
float dayTotalUsedkWh;
unsigned long pulseRate;

float multiplyer;                         // for test random etc
float momentPower4kW; 
//---------------------------------------------------------------------------------------------
void setup()
{
  // setup input pins here with pull_up, else (default) float
  pinMode( 2, INPUT_PULLUP);
  pinMode( 3, INPUT_PULLUP);
  pinMode( 4, INPUT_PULLUP);
  pinMode( 5, INPUT_PULLUP);
  pinMode( 6, INPUT_PULLUP);
  pinMode( 7, INPUT_PULLUP);
  pinMode( 8, INPUT_PULLUP);
  pinMode( 9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
  pinMode(CTS, INPUT_PULLUP);    // if CTS is HIGH allow transmitting results to AWI Master
 
  Serial.begin(115200);       //standard serial
  Serial.println("{L}Start AWI Slave Smart Meter V2.2");
  
  DDRD = DDRD | B00000000;    // see Youtube "Arduino basics 103"  start at ca 3 min Port manipulation
  DDRB = DDRD | B00000000;
  ltime = micros();           // at this moment we start reading the ports

}

void loop()
{

  la = a;                    //last register a used to detect input change 
  lb = b;                    //last register b used to detect input change

  //--------------------------------------------------------------------
  // Read from input registers
  //--------------------------------------------------------------------
  a = PIND >> 2;             //read digital inputs 2 to 7 really fast
  b = PINB;                  //read digital inputs 8 to 13 really fast
  time = micros();
  if (la!=a || lb!=b)
  {
    //--------------------------------------------------------------------
    // Detect pulses from register A
    //--------------------------------------------------------------------
    p[2].pulse(0,a,time);                //digital input 2
    p[3].pulse(1,a,time);                //    ''        3
    p[4].pulse(2,a,time);                //    ''        etc
    p[5].pulse(3,a,time);
    p[6].pulse(4,a,time);
    p[7].pulse(5,a,time);

    //--------------------------------------------------------------------
    // Detect pulses from register B
    //--------------------------------------------------------------------
    p[8].pulse(0,b,time);                //digital input 8
    p[9].pulse(1,b,time);                //etc
    p[10].pulse(2,b,time);
    p[11].pulse(3,b,time);
    p[12].pulse(4,b,time);
    p[13].pulse(5,b,time);
  }

  //--------------------------------------------------------------------
  // Spit out data every printTime sec (time here is in microseconds)
  //--------------------------------------------------------------------

  if (digitalRead(CTS)==HIGH)              // HIGH=allow transmission to master
  {
    deltaTime=time-ltime;
    if (deltaTime>printTime) 
    {
      dayTotalUsedkWh=0;
      totalMomentPowerkW=0;

// print fixed output like P1 port on smart meter
      Serial.print("/KMP5 AWI-12-V2_slave_SMART_METER V2.2 ");     // dummy meterinfo
      Serial.print(time);
      Serial.println();
      Serial.println();
        
      Serial.println("0-0:96.1.1(99990001)");                 // dummy meternumber
       
      Serial.println("0-0:96.14.0(0001)");                    // tarief indicator = 1

      for (curMeter=2;curMeter<=lastMeter;curMeter++)
      {
        pulseRate = (unsigned long)p[curMeter].rate(time);
        if (pulseRate == 0)                                     // calculate power from pulse rate (ms) and truncate to whole Watts
        {
          momentPowerkW = 0;   // avoid overflow assume no Usage
        } 
        else 
        {
          momentPowerkW = ( 1800000. / pulseRate );             // NB 1800000. = 2000 pulse/kWhour, pulseRate is in microseconds
        }

// Simulate other S0 meters for testing purposes
/*		
        if (curMeter==4)                                        // DEBUG power meter 3
        {
          momentPowerkW=momentPower4kW;
        } 

        if (curMeter==3)                                        // DEBUG generate random power
        {
          multiplyer=(float) random(5, 40)/10;
          momentPower4kW=momentPowerkW*2;
          momentPowerkW=momentPowerkW*multiplyer;
        }
*/

// Note: following computations not needed for P1 output, they are part of the original Master of the AWI project
/*        
        totalMomentPowerkW+=momentPowerkW;                      // running total momentary used power in W 
        contributePowerkW=momentPowerkW*deltaTime/3600000000.;  //3600000000. = result in kWh 
        p[curMeter].dayUsagekWh+=contributePowerkW;
        dayTotalUsedkWh+=p[curMeter].dayUsagekWh;               // running total power used today in kWh
*/

/*        
// Original output is build JSON: for all counters print Count (W), Count Accum(W), Average ms
// Format {"m":meter,"c":count,"r":rate, "cA":countAccum}
        Serial.print("{\"m\":"); 
        Serial.print(curMeter-1);             //Print meter number
        Serial.print(",\"c\":"); 
        Serial.print(p[curMeter].count);      //Print pulse count
        Serial.print(",\"r\":");             
        Serial.print(pulseRate);              //Print pulse rate
        p[curMeter].countAccum += p[curMeter].count;  //Increment and print count accumulator to allow for error checking at client side;
        Serial.print(",\"cA\":"); 
        Serial.print(p[curMeter].countAccum); 
        Serial.println("}");
*/

// print variable output like P1 port on smart meter
        
        Serial.print("1-0:");                                   //actual received power meter x
        Serial.print(curMeter-1);
        Serial.print(".8.1(");
        dtostrf((float)p[curMeter].dayUsagekWh, 8, 3, data11);                   
        Serial.print(data11);
        Serial.println("*kWh)");
         
        Serial.print("1-0:");                                   // actual power deliverd +P meter x
        Serial.print(curMeter-1);
        Serial.print(".7.0("); 
        dtostrf(momentPowerkW, 8, 3, data11);       
        Serial.print(data11);
        Serial.println("*kW)");
//
        Serial.print("0-0:96.13.0(Meter= ");                     // for debug text message
        Serial.print(curMeter-1);                                // Print meter number
        Serial.print(" Count=");
        Serial.print(p[curMeter].count);
//        if (curMeter==3)                                        // DEBUG generated random power information
//        {
//          Serial.print(" multiplyer= ");
//          Serial.print(multiplyer);
//        }
        Serial.println(")");
//       
        p[curMeter].count = 0;                //Reset count (we just send count increment)
        p[curMeter].prateAccum = 0;           //Reset accum so that we can calculate a new average

      } // end for (curMeter=2;CurMeter<=lastMeter;curMeter++)
		  
// COMPUTED TOTALS, COMMENT OUT OR NOT
/*
      Serial.print("1-0:");                                   // print computed total of received power
      Serial.print(lastMeter);
      Serial.print(".8.1(");
      dtostrf(dayTotalUsedkWh, 8, 3, data11);                   
      Serial.print(data11);
      Serial.println("*kWh)");

      Serial.print("1-0:");                                   // actual power deliverd +P meter x
      Serial.print(lastMeter);
      Serial.print(".7.0("); 
      dtostrf(totalMomentPowerkW, 8, 3, data11);       
      Serial.print(data11);
      Serial.println("*kW)");
*/
// END COMPUTED TOTALS, COMMENT OUT OR NOT

      Serial.println("!");                                    // end of data message
      Serial.println();
    
      ltime = time;                                           //save last Printed timer  
    }
  } // end (digitalRead(CTS)==HIGH) 
  else 
  if (ctsTime==0||time>ctsTime+deltaCtsTime)
  {
    Serial.println("{L}CTS low");
    ctsTime=time;
  }
} // end void loop()

// library for pulse, originally in separate file 

//-----------------------------------------------------------------------------------
//Gets a particular input state from the register binary value
// A typical register binary may look like this:
// B00100100
// in this case if the right most bit is digital pin 0
// digital 2 and 5 are high
// The method below extracts this from the binary value
//-----------------------------------------------------------------------------------
#define BIT_TST(REG, bit, val)( ( (REG & (1UL << (bit) ) ) == ( (val) << (bit) ) ) )

//-----------------------------------------------------------------------------------
// Method detects a pulse, counts it, finds its rate, Class: PulseOutput
//-----------------------------------------------------------------------------------
boolean PulseOutput::pulse(int pin, int a, unsigned long timeIn)
{
  ld = d;                                    //last digital state = digital state
   
  if (BIT_TST(a,pin,1)) d = 1; else d = 0;   //Get current digital state from pin number
   
  // if (ld==0 && d==1)                      // no internal pull_up if state changed from 0 to 1: internal pull-up inverts state
  if (ld==1 && d==0)                         //pull_up f state changed from 0 to 1: internal pull-up inverts state
  {
    count++;                                 //count the pulse
     
    // Rate calculation
    lastTime = time;           
    time = timeIn ;            // correction to allow for processing
    prate = (time-lastTime);// - 400;          //rate based on last 2 pulses
                                                //-190 is an offset that may not be needed...??
    prateAccum += prate - 2000;                     //accumulate rate for average calculation
     
    return 1;
   }
   return 0;
}


//-----------------------------------------------------------------------------------
// Method calculates the average rate based on multiple pulses (if there are 2 or more pulses)
//-----------------------------------------------------------------------------------
unsigned long PulseOutput::rate(unsigned long timeIn)
{
 if (count > 1)
 {
   prate = prateAccum / count;                          //Calculate average
 } else 
 {
 
 if ((timeIn - lastTime)>(prate*2)) prate = 0;}         //Decrease rate if no pulses are received
                                                        //in the expected time based on the last 
                                                        //pulse width.
 return prate; 
}

 
