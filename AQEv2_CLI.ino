/*** 
        Command Line Interface for AQE v2 built on Bitlash
        Copyright (C) 2014 Victor Aprea for Wicked Device LLC     
***/

/*** Derived From:

	userfunctions.pde:	Bitlash User Functions Demo Code

	Copyright (C) 2008-2012 Bill Roy

	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:
	
	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.

***/
#include "bitlash.h"
#include "Wire.h"
#include "DHT.h"
#include "MCP342x.h"
#include "LMP91000.h"

DHT dht(A1, DHT22);
MCP342x adc; // default address is 0x68
LMP91000 lmp91000;

// CLI user function names
// function names must be no more than 11 characters long
const char * func_scani2c = "scani2c";
const char * func_select_no2 = "no2";
const char * func_select_co = "co";
const char * func_select_ozone = "o3";
const char * func_humidity = "humidity";
const char * func_temp = "temp";
const char * func_adc = "adc";
const char * func_log = "log";

void scanI2CBus(byte from_addr, byte to_addr, 
                void(*callback)(byte address, byte result) );

numvar scani2c(void) { 
  scanI2CBus(1, 127, scanFunc);
}

void selectNoSlot(void){
        digitalWrite(7, LOW);
        digitalWrite(9, LOW);
        digitalWrite(10, LOW);  
}

numvar selectSlot1(void){
        selectNoSlot();
        digitalWrite(10, HIGH);    
}

numvar selectSlot2(void){
        selectNoSlot();
        digitalWrite(9, HIGH);  
}

numvar selectSlot3(void){
        selectNoSlot();
        digitalWrite(7, HIGH);
}

numvar temperature(void){
  float t = dht.readTemperature();
  if(isnan(t)){
    Serial.println("Failed to read from DHT");    
  }
  else{
    Serial.print(t, 2);  
    Serial.println(" *C");
  }
}

numvar humidity(void){
  float h = dht.readHumidity(); 
  if(isnan(h)){
    Serial.println("Failed to read from DHT");    
  }
  else{
    Serial.print(h, 2);  
    Serial.println(" %");
  }
}

numvar read_adc(void){
  long value = 0;
  MCP342x::Config status;
  // Initiate a conversion; convertAndRead() will wait until it can be read
  uint8_t err = adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot,
				   MCP342x::resolution16, MCP342x::gain1,
				   1000000, value, status);
  if (err) {
    Serial.print("Convert error: ");
    Serial.println(err);
  }
  else {
    Serial.print("Value: ");
    Serial.println(value);
  }  
}

numvar datalog(void){
  long value = 0;
  MCP342x::Config status;
  
  Serial.println("Time\tNO2\tCO\tO3\tTemp\tHum");
  while(1){       
    
    Serial.print(millis());
    Serial.print("\t");
    
    selectSlot1();
    adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot, 
      MCP342x::resolution16, MCP342x::gain1, 1000000, value, status);    
    Serial.print(value);
    Serial.print("\t");
    
    selectSlot2();
    adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot, 
      MCP342x::resolution16, MCP342x::gain1, 1000000, value, status);    
    Serial.print(value);    
    Serial.print("\t");
    
    selectSlot3();
    adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot, 
      MCP342x::resolution16, MCP342x::gain1, 1000000, value, status);    
    Serial.print(value);            
    Serial.print("\t");  

    float t = dht.readTemperature();        
    
    Serial.print(t, 2);                
    Serial.print("\t");  

    float h = dht.readHumidity();

    Serial.print(h, 2);                
    Serial.print("\t");      
    
    Serial.println();

    delay(1000);
  }
}

void setup(void) {
        // initialize the slot select pins to "not selected"
        pinMode(7, OUTPUT);  digitalWrite(7, LOW);
        pinMode(9, OUTPUT);  digitalWrite(9, LOW);
        pinMode(10, OUTPUT); digitalWrite(10, LOW);
        
        // fire up the i2c bus
        Wire.begin();

        selectSlot1(); //NO2
        lmp91000.configure( 
            LMP91000_TIA_GAIN_350K | LMP91000_RLOAD_10OHM,
            LMP91000_REF_SOURCE_EXT | LMP91000_INT_Z_67PCT 
                  | LMP91000_BIAS_SIGN_NEG | LMP91000_BIAS_8PCT,
            LMP91000_FET_SHORT_DISABLED | LMP91000_OP_MODE_AMPEROMETRIC                  
        );        

        selectSlot2(); //CO
        lmp91000.configure( 
            LMP91000_TIA_GAIN_350K | LMP91000_RLOAD_10OHM,
            LMP91000_REF_SOURCE_EXT | LMP91000_INT_Z_20PCT 
                  | LMP91000_BIAS_SIGN_POS | LMP91000_BIAS_1PCT,
            LMP91000_FET_SHORT_DISABLED | LMP91000_OP_MODE_AMPEROMETRIC                  
        );        
        
        selectSlot3(); //O3
        lmp91000.configure( 
            LMP91000_TIA_GAIN_350K | LMP91000_RLOAD_10OHM,
            LMP91000_REF_SOURCE_EXT | LMP91000_INT_Z_67PCT 
                  | LMP91000_BIAS_SIGN_NEG | LMP91000_BIAS_1PCT,
            LMP91000_FET_SHORT_DISABLED | LMP91000_OP_MODE_AMPEROMETRIC                  
        );        
        
        // initialize the DHT22
        dht.begin();
                
        // kick off bitlash
	initBitlash(115200);		// must be first to initialize serial port

        // bind user functions to CLI names
        // careful out of the box, bitlash allows 20 user functions (MAX_USER_FUNCTIONS)
	addBitlashFunction(func_scani2c, (bitlash_function) scani2c);
	addBitlashFunction(func_select_no2, (bitlash_function) selectSlot1);
	addBitlashFunction(func_select_co, (bitlash_function) selectSlot2);
	addBitlashFunction(func_select_ozone, (bitlash_function) selectSlot3);
        addBitlashFunction(func_temp, (bitlash_function) temperature);
        addBitlashFunction(func_humidity, (bitlash_function) humidity);
        addBitlashFunction(func_adc, (bitlash_function) read_adc);
        addBitlashFunction(func_log, (bitlash_function) datalog);
}

void loop(void) {
	runBitlash();
}

