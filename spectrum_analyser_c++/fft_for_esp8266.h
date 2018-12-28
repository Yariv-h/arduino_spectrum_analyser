/*
  Spectrum analyser for Florian's LED display. 
  Author: Benjamin Negrevergne.
  Based on example from the  FFT libray (Copyright (C) 2014 Enrique Condes).
*/

#ifndef FFT_For_ESP8266_h
#define FFT_For_ESP8266_h

#include "Arduino.h"

//#define NDEBUG // Uncomment for production

typedef uint8_t output_t; 

class FFT_For_ESP8266 {
  public:

  /* ctor */
  FFT_For_ESP8266(short analogPin, int numSamples, 
		  int displayWidth, int displayHeight, 
		  double *buffer = NULL, int numBars = 0, 
		  int numLines = 0, int barWidth = 3, int skipLine = 1); 

  /* Main functions */
  void sampleFromADC();

  /* Compute FFT from sampled data in data, and store magnitude for each band back into data (only the first size / 2 elements are used) */
  void computeFFT();

  /* Build the encoded graph from the fft output data.
   *  - The total number of columns in the graph is numCols * colWidth.
   *  - numRows should be 8 for a display with 8 leds
   */
  void buildGraph(output_t *out);

  /* Same as before, but uses internal storage to save memory
   * Use this version unless you have a reason to do otherwise
   */
  output_t *buildGraph();

  /* Print the output of build graph on Serial. Warning totalNumCols
   * is numCols * colWidth (not just numCols).  Does nothing if NDEBUG
   * is defined
  */
  void printGraph(output_t *graphData);

  /* Use this method instead of the previous one if you have called
   * buildGraph without any argument */ 
  void printGraph();



  private:
  


  const short _analogPin;
  const int _numSamples; 
  const int _displayWidth; 
  const int _displayHeight;
  int _numBars; 
  int _numLines; 
  int _barWidth; 
  int _skipCol; 
  const double _xScaleThreshold = 0.2; // switch from linear to logscale at x %
  const int _xScalePower = 2;  // base of the logarithmic x scale 
// buffer values to avoid recomputations
  double _xScaleFactor; 
  int _xScaleBar0; 
  int _xScaleNumBands0; 

  arduinoFFT _fft; 
  double *_data; 
  double *_dataImg; 

  /* skip first n bands  (lower frequencies) */
  static const short _skipLowBands = 2; 

  /* Parameters for the smooth y scaling ... 
   * (see comment on smoothMax() in this file for more details) */
  static const short _windowSize = 8;
  static const short _frameGroupSize = 8;

  /* Variables for the smooth y scaling */
  double _previousValues[_windowSize]; 
  double _previousSum; 
  short _count;

  /* Parameters for x log scaling */
  double _scalePower; 

  #ifndef NDEBUG
  /* Timing */
  long _t0;
  #endif


  /* Helper functions (do not export) */
  double maxv(double *data, int size);

  /* Smooth scaling ...
   *
   * The vertical scale of the graph adjusted dynamically based on a
   * maximum value which depends on the average maxima computed over a
   * sliding window of size _windowSize.
   *
   * In order to increase the time span of the sliding window without
   * using too much extra RAM, frames are grouped together and a local
   * maxima is computed for each group of frames. The size of each group
   * is controled by _frameGroupSize
   *
   * The final maximum is computed with the following formula 
   * max( max(current_frame), avg(max(passed_n_frame_group)) ) 
   * 
   * Increasing _windowSize will increase the smoothing but will also
   * use more memory. Increasing _frameGroupSize will achieve a
   * similar result without incrinsing the memory usage, but may
   * result in more abrut changes of the scaling which can be
   * noticeable.
   */
  double smoothMax(double *data, int size); 
  void startSampling();
  void printSamplingInfo(double *data, int size);
  void printVector(double *vData, int bufferSize, uint8_t scaleType);
  output_t encodeBar(output_t val);
  /* Convert bar ids to band ids (in a non linear fashion) */
  int barsToBands(int barIndex, int numBars, int numBands);  


};

#endif
