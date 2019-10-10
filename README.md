## **Project Dutch Smart Meter P1 simulator**

This project uses an Arduino to create eg simulate output simular to the Dutch Smart Meters with a P1 output port. 
The Arduino accept as input 0, 1 or 2 S0 ports of kWh powermeter-pulse output meters. The output follows the 
specification layed down in "Dutch Smart Meter Requirements V5.0.2", which document is added for easy reference.

The P1 simulator was designed to test offline from the real world the integration of S0 meters, a P1 meter and the 
"Domoticz Open Source Home Automation System", im my situation running on a Raspberry PI 3B+. 

The Arduino scetch can easily be modified to "generate" your own S0 input pulses to test different kWh simulations
for your own target applications. See comments in the scetch.


## **The following files are available:**

-  "Simulate_Smart_Meter_P1_V2.2.ino"               The Arduino scetch
-  "Dutch Smart Meter Requirements V5.0.2.pdf"      The original specs of a Dutch Smart Meter with an P1 port
-  "Output_example.txt"                             An example of the generated serial-ouput of simulated P1 port


## **Prerequisites:**

- This is an Arduino based project. You need an Arduino to run it. I run this project on a Arduino Pro Mini 5V/16Ghz 
  connected to an FTDI serial-to-USB interface, which is hooked up to or my PC or an USB port of the Raspberry PI.
- You must be able to download the compiled sketch into your Arduino, therefore
- You must have installed a recent version of the Arduino IDE.


## **Installing:**

You must copy the map Simulate_Smart_Meter_P1_V2.2 and the file Simulate_Smart_Meter_P1_V2.2.ino to compile this sketch.


## **Author:**

heinohein


**The sketch is based on the work of:**

Trystan Lea and "AWI" using their scetch "Arduino Pulse Counting Sketch for counting pulses from up to 12 
pulse output meters". See also Power/ Usage sensor - multi channel - local display on MySensors Forum
https://forum.mysensors.org

They are credited and thanked for their example code, which gave me ideas for this project
