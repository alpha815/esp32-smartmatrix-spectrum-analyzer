# esp32-smartmatrix-spectrum-analyzer
this is Arduino sketch for esp32 spectrum analyzer  
this is just a demonstration of fast led library, smartmatrix3 and Arduino FFT library working together to display frequency bands on 32x16 RGB led matrix.
but this code can easily be adapted for any display resolution. This code is heavily adapted from Scott Marley code for ws2812 leds.
FFT is performed by esp32 by sampling audio signal on PIN-35 at sampling rate of 1024.i heavly adopted this code from Scott Marley "ESP32 spectrum analyzer VU meter using arduinoFFT and a FastLED matrix" that is compatible for WS2812 based MATRIX. It is bassed on fastled libraray that is why it is fairly easily to adopt it for SMARTMATRIX3 library. This sketch contain five audio reactive patterns and a static pattern,it is not audio reactive.  which is example of fastled library. I will try to add more static modes if i had time.  patters can be switched between by pressing   button of esp32 named  as BOOT. 
i will add few links for you to get going.
Scott Marley video: "https://www.youtube.com/watch?v=Mgh2WblO5_c"
"https://github.com/s-marley/ESP32_FFT_VU"
what is not working :
waterfall pattern from original sketch was not working due to improper" XY " coordination that's why i deleted it.
display is upside down I tried few things but things got worse that is why I left it as it is.
switching between patterns is not working properly it get stuck on last pattern and then you have to reset esp32 there might be work around for this by setting  "bool autoChangePatterns = false;"  to true but i did not tested it.(if someone can fix it it will be highly appreciated)
you may need to tune few parameters based on how you feed audio signal to esp32 play around with amplitude and noise value .
I am using this circuit for line in:
"https://github.com/atuline/WLED/raw/assets/media/WLED_Line_In_Wiring.png"
and it works well, i also tried MAX9814 but due to bad frequency response (or something else) it does not work very well.
i am not a programmer but i will try to help if you need.

libraries :
smartmatrix3:
https://github.com/pixelmatix/SmartMatrix
fastled:
https://github.com/FastLED/FastLED
arduinoFFT:
https://github.com/kosme/arduinoFFT
