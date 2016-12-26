
#include <arduinoFFT.h>

#include "fft_for_esp8266.h"

/* Required for speeding up the ADC */ 
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

void setup()
{

  Serial.begin(115200);
  Serial.println("Ready");
  pinMode(0, INPUT);

  /* Speedup the ADC */
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);
}

void loop() 
{

  FFT_For_ESP8266 fft(0, 128, 48, 8);

  for(;;){

    double data[128]; 
    double dataImg[128] = {0}; 
    uint8_t bars[48] = {0}; 
    fft.sampleFromADC(data);
    fft.computeFFT(data, dataImg); 
    fft.buildGraph(bars, data);
    fft.printGraph(bars);

    delay(1000);
  }
  

}
