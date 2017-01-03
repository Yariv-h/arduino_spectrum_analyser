#include <assert.h>

#include "fft_for_esp8266.h"

/* Fake signal generation */
//#define GENERATE_FAKE_SIGNAL 1 //Generate fake signal instead of reading from ADC
#define Theta 6.2831 //2*Pi
double signalFrequency = 2000;
double samplingFrequency = 5000;
uint8_t amplitude = 50;


/* Misc values for printVector */
#define SCL_INDEX 0x00
#define SCL_TIME 0x01
#define SCL_FREQUENCY 0x02

FFT_For_ESP8266::FFT_For_ESP8266(short analogPin, int numSamples,
				 int displayWidth, int displayHeight,
				 double *buffer,
				 int numBars, int numLines, int barWidth, int skipCol):
  _analogPin(analogPin), 
  _numSamples(numSamples), 
  _displayWidth(displayWidth), 
  _displayHeight(displayHeight),
  _numBars(numBars),
  _numLines(numLines),
  _barWidth(barWidth), 
  _skipCol(skipCol),
  _fft(arduinoFFT()),
  _previousValues({0}), 
  _previousSum(0), 
  _count(0){

  if(numBars == 0)
    // maximize the number of bars
    _numBars = (_displayWidth + skipCol) / (_barWidth + _skipCol); 

  if(numLines == 0)
    _numLines = displayHeight; 

  #ifndef NDEBUG
  
    Serial.print(F("Display:  "));
    Serial.print( _numBars );
    Serial.print(F(" bars of size "));
    Serial.print(_barWidth);
    Serial.print(F(" with ")); 
    Serial.print(_skipCol); 
    Serial.print(F(" blanks in between each bar."));
    Serial.println(); 
     
    if(_numBars * _barWidth + (_numBars - 1) * _skipCol  > _displayWidth){
      Serial.print(F("Error : The graph does not fit in the screen."));
      assert(false); 
    }
#endif 
    
    assert(_numLines <= _displayHeight);
    
    /* output_t should be big enough to store one column stored as zeros and ones */
    assert(sizeof(output_t) * 8 >= log(_displayHeight) / log(2));
    assert(_numBars <= _numSamples / 2);

    if(buffer == NULL){
      _data = new double[_numSamples];
    }
    _dataImg = new double[_numSamples];
    
    setXLogScale(); 

    /* Bar falling */
    _previousFrame = new int[ _numBars ];
    memset(_previousFrame, 0, sizeof(_previousFrame[0]) * _numBars );     



}

void FFT_For_ESP8266::sampleFromADC(){
  startSampling();

  #ifdef GENERATE_FAKE_SIGNAL
  double cycles = (((_numSamples-1) * signalFrequency) / samplingFrequency); //Number of signal cycles that the sampling will read
  for (uint8_t i = 0; i < _numSamples; i++){
    _data[i] = uint8_t((amplitude * (sin((i * (Theta * cycles)) / _numSamples))) / 2.0);// Build _data with positive and negative values
    //_data[i] = uint8_t((amplitude * (sin((i * (6.2831 * cycles)) / _numSamples) + 1.0)) / 2.0);// Build _data displaced on the Y axis to include only positive values
  }
  #else
  for(uint8_t i = 0; i < _numSamples; i++){
    _data[i] = (double)analogRead(_analogPin);
  }
  #endif

  printSamplingInfo(_data, _numSamples);

}

void FFT_For_ESP8266::computeFFT(){

  memset(_dataImg, 0, sizeof(double) * _numSamples); 

  _fft.Windowing(_data, _numSamples, FFT_WIN_TYP_HAMMING, FFT_FORWARD);  /* Weigh _data */
  //Serial.println(F("Weighed _data:"));
  //printVector(vReal, _numSamples, SCL_TIME);

  _fft.Compute(_data, _dataImg, _numSamples, FFT_FORWARD); /* Compute FFT */
  //Serial.println(F("Computed Real values:"));
  //printVector(vReal, _numSamples, SCL_INDEX);
  //Serial.println(F("Computed Imaginary values:"));
  //printVector(_dataImg, _numSamples, SCL_INDEX);

  _fft.ComplexToMagnitude(_data, _dataImg, _numSamples); /* Compute magnitudes */

  #ifndef NDEBUG
  Serial.println(F("Computed magnitudes:"));
  //  printVector(_data, (_numSamples >> 1), SCL_FREQUENCY);
  #endif

}


output_t *FFT_For_ESP8266::buildGraph(){
  output_t *graphData = (output_t *)_dataImg; 
  memset(_dataImg, 0, sizeof(double) * _numSamples); 
  buildGraph( graphData );
  return graphData; 
}

void FFT_For_ESP8266::buildGraph(output_t *out){
  assert(_numLines <= _displayHeight);
  assert(_numBars * _barWidth <= _displayWidth);

  int numBands = _numSamples / 2 - _skipLowBands;

  double scalingFactor = (_numLines + 1) / smoothMax(_data + _skipLowBands, numBands);

  // used to bring all values back in the
  // interval [0 - _numLines] (inclusive because
  // we can display 9 distinct values with 8 leds.)

  /* Loop through display columns */
  for(int i = 0; i < _numBars; i++){

    int bandStart = barsToBands(i, _numBars, numBands) + _skipLowBands; 
    int bandEnd = barsToBands(i + 1, _numBars, numBands) + _skipLowBands; 

    double barHeight = 0;
    for(int j = bandStart; j < bandEnd; j++){
      /* take the maximum value of all bands displayed by this column */
      barHeight = max(_data[j],  barHeight);
    }


    #ifndef NDEBUG
    Serial.print(F("Col "));
    Serial.print(i);
    Serial.print(F(" : Band from "));
    Serial.print(bandStart);
    Serial.print(F(" to "));
    Serial.print(bandEnd);
    Serial.print(F(": "));
    Serial.print(barHeight);
    Serial.println();
    #endif

    int colStart = i * (_barWidth + _skipCol);
    int colEnd = i * (_barWidth + _skipCol) + _barWidth; 
  
    double x = barHeight * scalingFactor;
    if(x < _previousFrame[i]) 
      x = _previousFrame[i] - 1; 
    _previousFrame[i] = x; 

    output_t outVal = encodeBar(x); 
    int j; 

    for(j = colStart ; j < colEnd; j++){
      out[j] = outVal;
    }

    if( i != _numBars - 1 ) // if not the last bar add blank
      for(; j < colEnd + _skipCol; j++){
	out[j] = encodeBar(0);
      }
  }
}

void FFT_For_ESP8266::printGraph(output_t *graphData){
  //#ifndef NDEBUG
  Serial.println(F("--------------- spectrum analyser input-------------------"));
  for(int i = 0; i < _displayWidth; i++){
    Serial.print(graphData[i], HEX);
  }
  Serial.println();

  Serial.println(F("------------------ spectrum analyser ---------------------"));
  for(int i = _displayHeight - 1 ; i >= 0; i--){
    for(int j = 0; j < _displayWidth; j++){
      if( graphData[j] & (1<<i) )
          Serial.print(F("#"));
      else
          Serial.print(F(" "));
    }
    Serial.println();
  }
  for(int i = 0; i < _displayWidth; i++)
    Serial.print(F("-"));
  Serial.println();
  //#endif
}

void FFT_For_ESP8266::printGraph(){
  output_t *graphData = (output_t *)_dataImg;
  printGraph(graphData); 
}


output_t FFT_For_ESP8266::encodeBar(output_t val){

  if(val >= _numLines) {
    // Serial.println((1<<_numLines) - 1, HEX);
    return (1<<_numLines) - 1; // all led on.
  }

  output_t res = 0;
  for(output_t i = 0; i < val; i++){
    res |= (1<<i);
  }
  return res;
}

/* Compute the maximum value of a vector of size > 0 */
double FFT_For_ESP8266::maxv(double *data, int size){
  assert(size != 0);
  int i; 
  double max = data[i];
  for(i = 1; i < size; i++){
    max = data[i]>max?data[i]: max;
  }
  return max;
}

double FFT_For_ESP8266::smoothMax(double *data, int size){
  /* every 16 frames, shift all values by one, drop the oldest value and set the new value to 0 */

  if( _count++ % _frameGroupSize  == 0) {

    // Substract oldest max, add newst max
    _previousSum -= _previousValues[0]; 
    _previousSum += _previousValues[_windowSize-1];

    // shift all values by one in previousValues
    for(int i = 1; i < _windowSize; i++){
      _previousValues[i-1] = _previousValues[i];
    }

    // Reset most recent max to 0.
    _previousValues[ _windowSize -1 ] = 0; 
  }

  // set new value
  double currentMax = maxv(data, size); 
  currentMax = currentMax>500?currentMax:500; //TODO make it a parameter

  _previousValues[ _windowSize - 1 ] = max(_previousValues[ _windowSize - 1 ],
					   currentMax);
  printVector(_previousValues, _windowSize, SCL_INDEX); 
 
  double avgMax = (_previousSum + _previousValues[ _windowSize - 1 ]) / _windowSize;


  #ifndef NDEBUG
  Serial.print(F("Smoothmax: "));
  Serial.println(max(avgMax, currentMax)); 
  #endif 

  Serial.println(size); 

  return max(avgMax, currentMax);  
}


void FFT_For_ESP8266::startSampling(){
#ifndef NDEBUG
    _t0 = micros();
#endif
}

void FFT_For_ESP8266::printSamplingInfo(double *data, int size){
#ifndef NDEBUG
    float t = (micros() - _t0) / 1.0E3; // time (ms)

    Serial.print(F("Sampling time : "));
    Serial.println(t);

    Serial.print(F("Time per sample (ms) : "));
    Serial.println(t/size);

    Serial.print(F("Sampling frequency (hz) : "));
    Serial.println((1 / (t/1E3)) * size);

    Serial.print(F("Max ADC value: "));
    Serial.println(maxv(data, size));
#endif
}


void FFT_For_ESP8266::printVector(double *vData, int bufferSize, uint8_t scaleType)
{
#ifndef NDEBUG
  for (int i = 0; i < bufferSize; i++)
  {
    double abscissa;
    /* Print abscissa value */
    switch (scaleType)
    {
      case SCL_INDEX:
        abscissa = (i * 1.0);
  break;
      case SCL_TIME:
        abscissa = ((i * 1.0) / samplingFrequency);
  break;
      case SCL_FREQUENCY:
        abscissa = ((i * 1.0 * samplingFrequency) / _numSamples);
  break;
    }
    Serial.print(abscissa, 6);
    Serial.print(F(" "));
    Serial.print(vData[i], 4);
    Serial.println();
  }
  Serial.println();
#endif
}

int FFT_For_ESP8266::barsToBands(int barIndex, int numBars, int numBands){
  
  /* 1 bars maps to one band for first bars (lower frequencies) */
  if( barIndex <= _startLogScale){ 
    //    Serial.print(F("LIN SCALE Bar/band index ")); 
    //    Serial.println(barIndex); 
    return barIndex; 
  }
  else if ( barIndex == numBars ){ /* save this computation */ 
    return numBands;
  }
  /* Log scale for remaining bars 
   * I.e. maps on bar on n bands  with n growing exponentially with the bar index
   */
  else{
    double exponent = barIndex - _startLogScale; 
    int res = min(  _startLogScale + pow( _logScaleBase , exponent ), // 
		    numBands); 

#ifndef NDEBUG
    Serial.print(F("LOG SCALE barindex  "));
    Serial.print(barIndex); 
    Serial.print(F(" base ")); 
    Serial.print(_logScaleBase);
    Serial.print(F(" exponent "));
    Serial.print( exponent );
    Serial.print(F(" pow "));
    Serial.print(pow(_logScaleBase, exponent));
    Serial.print(F(" result ")); 
    Serial.println(res); 
#endif 

    return res; 
  }
}

void FFT_For_ESP8266::setXLogScale(uint8_t startLogScale, double base){
  _startLogScale = startLogScale;
  _logScaleBase = base; 
}

void FFT_For_ESP8266::setXLogScale(){

  /* set start logscale  */

  _startLogScale = 0.8 * _numBars;

  /* guess the best log base for the scale*/ 

  int numBands = _numSamples / 2 - _skipLowBands; 

  int remainingBars = _numBars - _startLogScale; 
  int remainingBands = numBands - _startLogScale;

  /* Maximum base value, i.e the base such that that base^bars = bands */
  double base = pow(2, (log(remainingBands) / log(2)) / remainingBars); 

  /* min base, garantees that the diff between two bars is at least one band */
  double minBase = log(remainingBands) / log(remainingBars); 

  if(base < minBase){
    _logScaleBase = minBase;
    Serial.println(F("Warning: xLogScale "));
    Serial.println(F("Cannot find a correct base for the log scale, strange bahaviours are to be expected (bars always set to zero). Change the graph parameters to fix this."));
  }
  else{
    _logScaleBase = base < 2 ? base : 2 ; // Do go above base 2 for logscale
  }
  
#ifndef NDEBUG
  Serial.println(F("Log scale parameters"));
  Serial.print(F("minbase ")); 
  Serial.print(minBase);
  Serial.print(F(" maxbase ")); 
  Serial.println(base);
  Serial.print(F(" base ")); 
  Serial.print(_logScaleBase);
  Serial.println(); 
#endif 
  
}
