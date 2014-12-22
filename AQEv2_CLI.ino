/*** 
        Command Line Interface for AQE v2 built on Bitlash
        Copyright (C) 2014 Victor Aprea for Wicked Device LLC     
***/

#include "bitlash.h"
#include "DHT.h"
#include "MCP342x.h"
#include "LMP91000.h"
#include "WildFire.h"
#include <avr/eeprom.h>
#include <Wire.h>
#include <SD.h>

const int sdChipSelect = 16;

WildFire wf;
DHT dht(A1, DHT22);
MCP342x adc; // default address is 0x68
LMP91000 lmp91000;
char filename[64] = "";
boolean sdcard_present = true;
boolean stop_log = false;

#define NUM_SAMPLES_TO_AVERAGE (32)
#define NO2_INDEX  (0)
#define CO_INDEX   (1)
#define O3_INDEX   (2)
#define TEMP_INDEX (3)
#define HUM_INDEX  (4)
float gas_averaging_window[5][NUM_SAMPLES_TO_AVERAGE];
uint8_t gas_averaging_window_index = 0;
uint8_t num_values_collected = 1; // so as not to average empty buffer space

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
const char * func_getfilename = "getlog";
const char * func_setfilename = "setlog";
const char * func_stoplog = "stoplog";
const char * func_dumplog = "dumplog";
const char * func_ls = "sdls";
const char * func_rm = "sdrm";

void scanI2CBus(byte from_addr, byte to_addr, 
                void(*callback)(byte address, byte result) );

numvar scani2c(void) { 
  scanI2CBus(1, 127, scanFunc);
}

void addValueToAveragingWindow(float value, uint8_t gas_index){
  gas_averaging_window[gas_index][gas_averaging_window_index] = value;
}

void advanceAveragingWindowIndex(void){
  gas_averaging_window_index++;
  if(gas_averaging_window_index >= NUM_SAMPLES_TO_AVERAGE){
    gas_averaging_window_index = 0;
  }
  
  if(num_values_collected < NUM_SAMPLES_TO_AVERAGE){
    num_values_collected++;
  }
}

float getWindowedAverageGasReading(uint8_t gas_index){
  float sum = 0.0f;
  uint8_t upper_bound = NUM_SAMPLES_TO_AVERAGE;
  if(num_values_collected < upper_bound) {
      upper_bound = num_values_collected;
  }
  
  for(uint8_t ii = 0; ii < upper_bound; ii++){
     sum += gas_averaging_window[gas_index][ii];
  }
  
  return sum / upper_bound;
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

numvar getfilename(void){
  Serial.print("Filename: ");
  Serial.println(filename); 
}

numvar setfilename(void){
  strncpy(filename, (char *) getarg(1), sizeof(filename));
  eeprom_write_block(filename, 0, 64);
}

void sdAppendRow(char * str){
  File dataFile = SD.open(filename, FILE_WRITE);  
  if(dataFile){
    dataFile.println(str);
    dataFile.close(); 
  } 
}

numvar stoplog(void){
  stop_log = true; 
}

numvar rm(void){
  Serial.print("Removing filename: ");
  Serial.print(filename);
  Serial.print("...");
  if(SD.exists(filename)){
    SD.remove(filename);
    Serial.println("done");
  }
  else{
    Serial.println("doesn't exist");        
  }
}

numvar ls(void){
  if(sdcard_present){      
    File root = SD.open("/");
    printDirectory(root, 0);
    root.close();
  }
}

void printDirectory(File dir, int numTabs) {
   while(true) {    
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

numvar dumplog(void){
  if(sdcard_present){
    File dataFile = SD.open(filename);
  
    // if the file is available, write to it:
    if (dataFile) {
      while (dataFile.available()) {
        Serial.write(dataFile.read());
      }
      dataFile.close();
    }    
  }
}

float burstSampleADC(void){
  #define NUM_SAMPLES_PER_BURST (16)
  MCP342x::Config status;
  int32_t burst_sample_total = 0;
  int32_t value = 0;
  for(uint8_t ii = 0; ii < NUM_SAMPLES_PER_BURST; ii++){
    adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot, 
      MCP342x::resolution16, MCP342x::gain1, 1000000, value, status);   
    burst_sample_total += value;
  }
  return (1.0f * burst_sample_total) / NUM_SAMPLES_PER_BURST;
}

numvar datalog(void){  
  stop_log = false;
  long value = 0;
  if(sdcard_present && strcmp(filename,"") == 0){
    Serial.println("Please use setlog(\"yourfilename.txt\") to set the filename to log to");
    return 0;
  }
  else if(sdcard_present){
    Serial.print("Logging to SD card ");
    getfilename();
  }
  
  const char * header_row = "Time[ms]\tNO2[ticks]\tNO2_filt[ticks]\tCO[ticks]\tCO_filt[ticks]\tO3[ticks]\tO3_filt[ticks]\tTemp[degC]\tTemp_filt[degC]\tHum[%]\tHum_filt[%]";
  sdAppendRow((char *) header_row);
  Serial.println(header_row);
 
  char buf[1024] = "";
  char temp[16] = "";
  float f = 0.0f;
  while(1){           
    const char * tab = "\t";
    buf[0] = 0; // clear the buffer
    
    uint32_t mil = millis();
    ltoa(mil, temp, 10);  
    Serial.print(mil);
    Serial.print(tab);
    strcat(buf, temp);
    strcat(buf, tab);
    
    selectSlot2(); 
    f = burstSampleADC(); 
    value = (long) f;
    Serial.print(value);
    Serial.print(tab);
    ltoa(value, temp, 10);
    strcat(buf, temp);
    strcat(buf, tab);    
    
    addValueToAveragingWindow(f, NO2_INDEX);    
    f = getWindowedAverageGasReading(NO2_INDEX);
    Serial.print(f, 2);                
    Serial.print("\t");  
    dtostrf(f, 7, 2, temp);
    strcat(buf, temp);
    strcat(buf, tab);    
    
    selectSlot3(); 
    f = burstSampleADC();
    value = (long) f;
    Serial.print(value);
    Serial.print(tab);
    ltoa(value, temp, 10);
    strcat(buf, temp);
    strcat(buf, tab);  

    addValueToAveragingWindow(f, CO_INDEX);    
    f = getWindowedAverageGasReading(CO_INDEX);
    Serial.print(f, 2);                
    Serial.print("\t");  
    dtostrf(f, 7, 2, temp);
    strcat(buf, temp);
    strcat(buf, tab);    
    
    selectSlot1(); // O3
    f = burstSampleADC();
    value = (long) f;
    Serial.print(value);
    Serial.print(tab);
    ltoa(value, temp, 10);
    strcat(buf, temp);
    strcat(buf, tab);  

    addValueToAveragingWindow(f, O3_INDEX);    
    f = getWindowedAverageGasReading(O3_INDEX);
    Serial.print(f, 2);                
    Serial.print("\t");  
    dtostrf(f, 7, 2, temp);
    strcat(buf, temp);
    strcat(buf, tab);     
    
    float t = dht.readTemperature();        
    addValueToAveragingWindow(t, TEMP_INDEX);    
    f = getWindowedAverageGasReading(TEMP_INDEX);
    Serial.print(t, 2);                
    Serial.print("\t");  
    dtostrf(t, 7, 2, temp);
    strcat(buf, temp);
    strcat(buf, tab);        
    Serial.print(f, 2);                
    Serial.print("\t");  
    dtostrf(f, 7, 2, temp);
    strcat(buf, temp);
    strcat(buf, tab);
    
    float h = dht.readHumidity();
    addValueToAveragingWindow(h, HUM_INDEX);    
    f = getWindowedAverageGasReading(HUM_INDEX);    
    Serial.print(h, 2);                
    Serial.print("\t");  
    dtostrf(h, 7, 2, temp);
    strcat(buf, temp);
    strcat(buf, tab);        
    Serial.print(f, 2);                
    Serial.print("\t");  
    dtostrf(f, 7, 2, temp);
    strcat(buf, temp);
    strcat(buf, tab);
    
    Serial.println();
    sdAppendRow(buf);
    
    for(uint8_t ii = 0; ii < 20; ii++){
      runBitlash();
    }
    
    advanceAveragingWindowIndex();    
    
    if(stop_log){
      return 0; 
    }
    delay(1000);
  }
}

void setup(void) {
        wf.begin();
        eeprom_read_block(filename, 0, 64); // read the filename into RAM
        // initialize the slot select pins to "not selected"
        pinMode(7, OUTPUT);  digitalWrite(7, LOW);
        pinMode(9, OUTPUT);  digitalWrite(9, LOW);
        pinMode(10, OUTPUT); digitalWrite(10, LOW);        
        
        // fire up the i2c bus
        Wire.begin();

        selectSlot2(); //NO2
        lmp91000.configure( 
            LMP91000_TIA_GAIN_350K | LMP91000_RLOAD_10OHM,
            LMP91000_REF_SOURCE_EXT | LMP91000_INT_Z_67PCT 
                  | LMP91000_BIAS_SIGN_NEG | LMP91000_BIAS_8PCT,
            LMP91000_FET_SHORT_DISABLED | LMP91000_OP_MODE_AMPEROMETRIC                  
        );        

        selectSlot3(); //CO
        lmp91000.configure( 
            LMP91000_TIA_GAIN_350K | LMP91000_RLOAD_10OHM,
            LMP91000_REF_SOURCE_EXT | LMP91000_INT_Z_20PCT 
                  | LMP91000_BIAS_SIGN_POS | LMP91000_BIAS_1PCT,
            LMP91000_FET_SHORT_DISABLED | LMP91000_OP_MODE_AMPEROMETRIC                  
        );        
        
        selectSlot1(); //O3
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

        if(!SD.begin(sdChipSelect)) {
          Serial.println("Card failed, or not present");
          Serial.print(">");
          sdcard_present = false;
        }   
        else{
          Serial.println("SD Card initialized");
          Serial.print(">");          
        }     

        // bind user functions to CLI names
        // careful out of the box, bitlash allows 20 user functions (MAX_USER_FUNCTIONS)
	addBitlashFunction(func_scani2c, (bitlash_function) scani2c);
	addBitlashFunction(func_select_no2, (bitlash_function) selectSlot2);
	addBitlashFunction(func_select_co, (bitlash_function) selectSlot3);
	addBitlashFunction(func_select_ozone, (bitlash_function) selectSlot1);
        addBitlashFunction(func_temp, (bitlash_function) temperature);
        addBitlashFunction(func_humidity, (bitlash_function) humidity);
        addBitlashFunction(func_adc, (bitlash_function) read_adc);
        addBitlashFunction(func_log, (bitlash_function) datalog);
        addBitlashFunction(func_getfilename, (bitlash_function) getfilename);                
        addBitlashFunction(func_setfilename, (bitlash_function) setfilename);
        addBitlashFunction(func_stoplog, (bitlash_function) stoplog);      
        addBitlashFunction(func_dumplog, (bitlash_function) dumplog);    
        addBitlashFunction(func_ls, (bitlash_function) ls);            
        addBitlashFunction(func_rm, (bitlash_function) rm);                         
        
        uint32_t start = millis();
        uint8_t ii = 9;
        Serial.println("Hit <Enter> within 10 seconds to prevent auto log");
        boolean auto_log = true;
        for(;;){           
           if(Serial.available()){
               auto_log = false;
               break;
           }
           
           Serial.print(ii--);
           Serial.print("...");
           delay(1000);
           
           if(millis() > start + 10000){
             break; 
           }                      
        }
        Serial.println();
        
        if(auto_log){
           datalog();
        }
}

void loop(void) {
	runBitlash();
}

