/***
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
#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif
#include "bitlash.h"

#include "Wire.h"
extern "C" { 
#include "utility/twi.h"  // from Wire library, so we can do bus scanning
}

// Scan the I2C bus between addresses from_addr and to_addr.
// On each address, call the callback function with the address and result.
// If result==0, address was found, otherwise, address wasn't found
// (can use result to potentially get other status on the I2C bus, see twi.c)
// Assumes Wire.begin() has already been called
void scanI2CBus(byte from_addr, byte to_addr, 
                void(*callback)(byte address, byte result) ) 
{
  byte rc;
  byte data = 0; // not used, just an address to feed to twi_writeTo()
  for( byte addr = from_addr; addr <= to_addr; addr++ ) {
    rc = twi_writeTo(addr, &data, 0, 1, 0);
    callback( addr, rc );
  }
}

// Called when address is found in scanI2CBus()
// Feel free to change this as needed
// (like adding I2C comm code to figure out what kind of I2C device is there)
void scanFunc( byte addr, byte result ) {
  if(result == 0){
    Serial.print("0x");
    if(addr < 16) Serial.print("0");
    Serial.println(addr, HEX); 
}

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

void setup(void) {
        pinMode(7, OUTPUT);  digitalWrite(7, LOW);
        pinMode(9, OUTPUT);  digitalWrite(9, LOW);
        pinMode(10, OUTPUT); digitalWrite(10, LOW);
        
        Wire.begin();
	initBitlash(115200);		// must be first to initialize serial port

	addBitlashFunction("scani2c", (bitlash_function) scani2c);
	addBitlashFunction("nox", (bitlash_function) selectSlot1);
	addBitlashFunction("co", (bitlash_function) selectSlot2);
	addBitlashFunction("ozone", (bitlash_function) selectSlot3);
}

void loop(void) {
	runBitlash();
}

