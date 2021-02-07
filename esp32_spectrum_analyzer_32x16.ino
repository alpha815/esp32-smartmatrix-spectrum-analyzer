/*
this is just a demonstration of fast led library, smartmatrix3 and arduino FFT library working togather to display frequency bands on 32x16 RGB led matrix.
but tthis code can easily be adapted for any display resolution.this code is heavily adapted from Scott Marley code for ws2812 leds.
*/
// (Heavily) adapted from https://github.com/G6EJD/ESP32-8266-Audio-Spectrum-Display/blob/master/ESP32_Spectrum_Display_02.ino
#include <SmartMatrix3.h>           // add smartmetrix3 library 
#include <FastLED.h>                // add fast led library
#include <arduinoFFT.h>             // add Arduino fft library
#include <EasyButton.h>              // easybutton library for switching patterns

#define SAMPLES         1024          // Must be a power of 2
#define SAMPLING_FREQ   40000         // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define amplitude   8500              // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define AUDIO_IN_PIN    35            // Signal in on this pin
#define BTN_PIN         0             // Connect a push button to this pin to change patterns
#define LONG_PRESS_MS   200           // Number of ms to count as a long press
#define BRIGHTNESS      200           // Brightness 0 - 255, but won't exceed current specified above
unsigned long newTime;
#define NUM_BANDS       32            // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE        4200           // Used as a crude noise filter, values below this are ignored
//smartmatrix3
#define COLOR_DEPTH 24                  // This sketch and FastLED uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 32;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 16;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 48;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_16ROW_MOD8SCAN;   // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE); // 
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE); // 

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);

#define BAR_WIDTH      (kMatrixWidth  / (NUM_BANDS - 1))  // If width >= 8 light 1 LED width per bar, >= 16 light 2 LEDs width bar etc
#define TOP            (kMatrixHeight - 0)                // Don't allow the bars to go offscreen
#define NUM_LEDS (kMatrixWidth*kMatrixHeight)

// Sampling and FFT stuff
unsigned int sampling_period_us;
byte peak[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // The length of these arrays must be >= NUM_BANDS
int oldBarHeights[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int bandValues[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// Button stuff
int buttonPushCounter = 0;
bool autoChangePatterns = false;
EasyButton modeBtn(BTN_PIN);

// FastLED stuff color palettes

DEFINE_GRADIENT_PALETTE( purple_gp ) {
  0,   0, 212, 255,   //blue
  255, 179,   0, 255
}; //purple
DEFINE_GRADIENT_PALETTE( outrun_gp ) {
  0, 141,   0, 100,   //purple
  127, 255, 192,   0,   //yellow
  255,   0,   5, 255
};  //blue
DEFINE_GRADIENT_PALETTE( greenblue_gp ) {
  0,   0, 255,  60,   //green
  64,   0, 236, 255,   //cyan
  128,   0,   5, 255,   //blue
  192,   0, 236, 255,   //cyan
  255,   0, 255,  60
}; //green
DEFINE_GRADIENT_PALETTE( redyellow_gp ) {
  0,   200, 200,  200,   //white
  64,   255, 218,    0,   //yellow
  128,   231,   0,    0,   //red
  192,   255, 218,    0,   //yellow
  255,   200, 200,  200
}; //white
CRGBPalette16 purplePal = purple_gp;
CRGBPalette16 outrunPal = outrun_gp;
CRGBPalette16 greenbluePal = greenblue_gp;
CRGBPalette16 heatPal = redyellow_gp;
uint8_t colorTimer = 0;

// XY code for serpentine matrix with input in top left this code need some fix display is upside down idk why
uint16_t XY( uint8_t x, uint8_t y) {
  uint16_t i;
    i = ((kMatrixHeight - 1 - y) * kMatrixWidth) + x;
  return i;
}
void setup() {
  //Serial.begin(115200);

  matrix.addLayer(&backgroundLayer);
  matrix.addLayer(&scrollingLayer);
  matrix.begin();
  matrix.setBrightness(BRIGHTNESS);


  modeBtn.begin();
  modeBtn.onPressed(changeMode);
  modeBtn.onPressedFor(LONG_PRESS_MS, startAutoMode);

  sampling_period_us = round(1000000 * (1.5 / SAMPLING_FREQ));
}

void changeMode() {
  //Serial.println("Button pressed"); // enable it for debuging
  autoChangePatterns = false;
  buttonPushCounter = (buttonPushCounter + 1) % 6;
}

void startAutoMode() {
  autoChangePatterns = true;
}

void loop() {
  rgb24 *buffer = backgroundLayer.backBuffer(); // smartmatrix3 fuction for backBuffer https://github.com/pixelmatix/SmartMatrix/blob/master/MIGRATION.md#porting-fastled-sketch-to-smartmatrix
  // Don't clear screen if waterfall pattern, be sure to change this is you change the patterns / order
  backgroundLayer.fillScreen({0, 0, 0});

  modeBtn.read();

  // Reset bandValues[]
  for (int i = 0; i < NUM_BANDS; i++) {
    bandValues[i] = 0;
  }

  // Sample the audio pin
  for (int i = 0; i < SAMPLES; i++) {
    newTime = micros();
    vReal[i] = analogRead(AUDIO_IN_PIN); // A conversion takes about 9.7uS on an ESP32

    vImag[i] = 0;
    while ((micros() - newTime) < sampling_period_us) {

      /* chill */
    }
  }

  // Compute FFT
  FFT.DCRemoval();
  FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(FFT_FORWARD);
  FFT.ComplexToMagnitude();

  // Analyse FFT results
  for (int i = 2; i < (SAMPLES / 2); i++) {    // Don't use sample 0 and only first SAMPLES/2 are usable. Each array element represents a frequency bin and its value the amplitude.
    if (vReal[i] > NOISE) {                    // Add a crude noise filter
      //Serial.println(vReal[i]);              // uncomment for debuging but will slow down display 

      
/*//      /32 bands, 280hz to 12kHz top band
      if (i <= 8 )           bandValues[0]  += (int)vReal[i];
      if (i > 8   && i <= 9  ) bandValues[1]  += (int)vReal[i];
      if (i > 9   && i <= 10  ) bandValues[2]  += (int)vReal[i];
      if (i > 10   && i <= 11 ) bandValues[3]  += (int)vReal[i];
      if (i > 11  && i <= 12 ) bandValues[4]  += (int)vReal[i];
      if (i > 12 && i <= 14 ) bandValues[5]  += (int)vReal[i];
      if (i > 14 && i <= 16) bandValues[6]  += (int)vReal[i];
      if (i > 16 && i <= 18) bandValues[7]  += (int)vReal[i];
      if (i > 18 && i <= 20) bandValues[8]  += (int)vReal[i];
      if (i > 20 && i <= 23 ) bandValues[9]  += (int)vReal[i];
      if (i > 23  && i <= 26 ) bandValues[10] += (int)vReal[i];
      if (i > 26  && i <= 29 ) bandValues[11] += (int)vReal[i];
      if (i > 29  && i <= 33) bandValues[12] += (int)vReal[i];
      if (i > 33 && i <= 37) bandValues[13] += (int)vReal[i];
      if (i > 37 && i <= 42) bandValues[14] += (int)vReal[i];

      if (i > 42   && i <= 47  ) bandValues[15]  += (int)vReal[i];
      if (i > 47   && i <= 53  ) bandValues[16]  += (int)vReal[i];
      if (i > 53   && i <= 60  ) bandValues[17]  += (int)vReal[i];
      if (i > 60   && i <= 68  ) bandValues[18]  += (int)vReal[i];
      if (i > 68   && i <= 76 ) bandValues[19]  += (int)vReal[i];
      if (i > 76  && i <= 86) bandValues[20]  += (int)vReal[i];
      if (i > 86  && i <= 97 ) bandValues[21]  += (int)vReal[i];
      if (i > 97  && i <= 110 ) bandValues[22]  += (int)vReal[i];
      if (i > 110  && i <= 124 ) bandValues[23]  += (int)vReal[i];
      if (i > 124  && i <= 140 ) bandValues[24] += (int)vReal[i];
      if (i > 140  && i <= 158 ) bandValues[25] += (int)vReal[i];
      if (i > 158  && i <= 178) bandValues[26] += (int)vReal[i];
      if (i > 178 && i <= 201) bandValues[27] += (int)vReal[i];
      if (i > 201 && i <= 227) bandValues[28] += (int)vReal[i];
      if (i > 227 &&  i <=  257) bandValues[29] += (int)vReal[i];
      if (i > 257 &&  i <=  290) bandValues[30] += (int)vReal[i];
      if (i > 290               ) bandValues[31] += (int)vReal[i];
      //Serial.println(i);                                       //// use only for debuging slows down the display


     /* 
        //16 bands, 12kHz top band
        if (i<=2 )           bandValues[0]  += (int)vReal[i];
        if (i>2   && i<=3  ) bandValues[1]  += (int)vReal[i];
        if (i>3   && i<=5  ) bandValues[2]  += (int)vReal[i];
        if (i>5   && i<=7  ) bandValues[3]  += (int)vReal[i];
        if (i>7   && i<=9  ) bandValues[4]  += (int)vReal[i];
        if (i>9   && i<=13 ) bandValues[5]  += (int)vReal[i];
        if (i>13  && i<=18 ) bandValues[6]  += (int)vReal[i];
        if (i>18  && i<=25 ) bandValues[7]  += (int)vReal[i];
        if (i>25  && i<=36 ) bandValues[8]  += (int)vReal[i];
        if (i>36  && i<=50 ) bandValues[9]  += (int)vReal[i];
        if (i>50  && i<=69 ) bandValues[10] += (int)vReal[i];
        if (i>69  && i<=97 ) bandValues[11] += (int)vReal[i];
        if (i>97  && i<=135) bandValues[12] += (int)vReal[i];
        if (i>135 && i<=189) bandValues[13] += (int)vReal[i];
        if (i>189 && i<=264) bandValues[14] += (int)vReal[i];
        if (i>264          ) bandValues[15] += (int)vReal[i];
*/             //// 80 to 12khz this need to be fixed 2nd and 4th band are not working
 if (i <= 2 )           bandValues[0]  += (int)vReal[i];
        if (i > 2   && i <= 3  ) bandValues[1]  += (int)vReal[i];
        if (i > 3   && i <= 3  ) bandValues[2]  += (int)vReal[i];
        if (i > 3   && i <= 4  ) bandValues[3]  += (int)vReal[i];
        if (i > 4   && i <= 4  ) bandValues[4]  += (int)vReal[i];
        if (i > 4  && i <= 5 ) bandValues[5]  += (int)vReal[i];
        if (i > 5  && i <= 6 ) bandValues[6]  += (int)vReal[i];
        if (i > 6  && i <= 7 ) bandValues[7]  += (int)vReal[i];
        if (i > 7  && i <= 9 ) bandValues[8]  += (int)vReal[i];
        if (i > 9  && i <= 10 ) bandValues[9]  += (int)vReal[i];
        if (i > 10  && i <= 12 ) bandValues[10] += (int)vReal[i];
        if (i > 12  && i <= 15 ) bandValues[11] += (int)vReal[i];
        if (i > 15  && i <= 17) bandValues[12] += (int)vReal[i];
        if (i > 17 && i <= 21) bandValues[13] += (int)vReal[i];
        if (i > 21 && i <= 25) bandValues[14] += (int)vReal[i];

        if (i > 25   && i <= 29  ) bandValues[15]  += (int)vReal[i];
        if (i > 29   && i <= 34  ) bandValues[16]  += (int)vReal[i];
        if (i > 34   && i <= 41  ) bandValues[17]  += (int)vReal[i];
        if (i > 41   && i <= 49  ) bandValues[18]  += (int)vReal[i];
        if (i > 49   && i <= 58 ) bandValues[19]  += (int)vReal[i];
        if (i > 58  && i <= 68 ) bandValues[20]  += (int)vReal[i];
        if (i > 68  && i <= 81 ) bandValues[21]  += (int)vReal[i];
        if (i > 81  && i <= 96 ) bandValues[22]  += (int)vReal[i];
        if (i > 96  && i <= 114 ) bandValues[23]  += (int)vReal[i];
        if (i > 114  && i <= 135 ) bandValues[24] += (int)vReal[i];
        if (i > 135  && i <= 161 ) bandValues[25] += (int)vReal[i];
        if (i > 161  && i <= 191) bandValues[26] += (int)vReal[i];
        if (i > 191 && i <= 226) bandValues[27] += (int)vReal[i];
        if (i > 226 && i <= 268) bandValues[28] += (int)vReal[i];
        if (i > 268 &&  i <=  318) bandValues[29] += (int)vReal[i];
        if (i > 318 &&  i <=  377) bandValues[30] += (int)vReal[i];
        if (i > 377               ) bandValues[31] += (int)vReal[i];     
    }
  }


  // Process the FFT data into bar heights
  for (byte band = 0; band < NUM_BANDS; band++) {

    // Scale the bars for the display
    int barHeight = bandValues[band] / amplitude;
    if (barHeight > TOP) barHeight = TOP;

    // Small amount of averaging between frames
    barHeight = ((oldBarHeights[band] * 1) + barHeight) / 2;

    // Move peak up
    if (barHeight > peak[band]) {
      peak[band] = min(TOP, barHeight);
    }

    // Draw bars
    switch (buttonPushCounter) {
      case 0:
        rainbowBars(band, barHeight);
        break;
      case 1:
        // No bars on this one
        break;
      case 2:
        purpleBars(band, barHeight);
        break;
      case 3:
        centerBars(band, barHeight);
        break;
      case 4:
        purpleBars(band, barHeight);
        break;
      case 5:
        pride();
        break;

    }

    // Draw peaks
    switch (buttonPushCounter) {
      case 0:
        whitePeak(band);
        break;
      case 1:
        outrunPeak(band);
        break;
      case 2:
        whitePeak(band);
        break;
      case 3:
        // No peaks
        break;
      case 4:
        // No peaks
        break;

    }

    // Save oldBarHeights for averaging later
    oldBarHeights[band] = barHeight;
  }

  // Decay peak
  EVERY_N_MILLISECONDS(60) {
    for (byte band = 0; band < NUM_BANDS; band++)
      if (peak[band] > 0) peak[band] -= 1;
    colorTimer++;
  }

  // Used in some of the patterns
  EVERY_N_MILLISECONDS(10) {
    colorTimer++;
  }

  EVERY_N_SECONDS(10) {
    if (autoChangePatterns) buttonPushCounter = (buttonPushCounter + 1) % 5;
  }

  backgroundLayer.swapBuffers();
}

// PATTERNS BELOW //

void rainbowBars(int band, int barHeight) {
  rgb24 *buffer = backgroundLayer.backBuffer();
  int xStart = BAR_WIDTH * band;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = 0; y < barHeight; y++) {
      buffer[XY(x, y)] = CRGB(CHSV((x / BAR_WIDTH) * (255 / NUM_BANDS), 255, 255));
    }
  }
}

void purpleBars(int band, int barHeight) {
  rgb24 *buffer = backgroundLayer.backBuffer();
  int xStart = BAR_WIDTH * band;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = 0; y < barHeight; y++) {
      buffer[XY(x, y)] = ColorFromPalette(purplePal, y * (255 / barHeight));
    }
  }
}

void changingBars(int band, int barHeight) {
  rgb24 *buffer = backgroundLayer.backBuffer();
  int xStart = BAR_WIDTH * band;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = 0; y < barHeight; y++) {
      buffer[XY(x, y)] = CRGB(CHSV(y * (255 / kMatrixHeight) + colorTimer, 255, 255));
    }
  }
}

void centerBars(int band, int barHeight) {
  rgb24 *buffer = backgroundLayer.backBuffer();
  int xStart = BAR_WIDTH * band;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    if (barHeight % 2 == 0) barHeight--;
    int yStart = ((kMatrixHeight - barHeight) / 2 );
    for (int y = yStart; y <= (yStart + barHeight); y++) {
      int colorIndex = constrain((y - yStart) * (255 / barHeight), 0, 255);
      buffer[XY(x, y)] = ColorFromPalette(heatPal, colorIndex);
    }
  }
}

void whitePeak(int band) {
  rgb24 *buffer = backgroundLayer.backBuffer();
  int xStart = BAR_WIDTH * band;
  int peakHeight = peak[band];
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    buffer[XY(x, peakHeight)] = (CRGB)CRGB::White;
  }
}

void outrunPeak(int band) {
  rgb24 *buffer = backgroundLayer.backBuffer();
  int xStart = BAR_WIDTH * band;
  int peakHeight = peak[band];
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    //leds[XY(x,peakHeight)] = CHSV(peakHeight * (255 / kMatrixHeight), 255, 255);
    buffer[XY(x, peakHeight)] = ColorFromPalette(outrunPal, peakHeight * (255 / kMatrixHeight));
  }
}
// here is a static pattern just for fun. i will add more of it some day.
void pride()
{
  rgb24 *buffer = backgroundLayer.backBuffer();
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CRGB(CHSV( hue8, sat8, bri8));

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    //nblend( buffer[pixelnumber], newcolor, 64);
    nblend((CRGB&)buffer[pixelnumber], newcolor, 64);
  }
}
