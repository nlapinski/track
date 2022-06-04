# track
cv_tracker
![image](https://user-images.githubusercontent.com/4634469/171978661-3c6d7938-62bb-4729-b8e4-85cc04b54b47.png)

depends on a custom MRAA dac interface for outputing control voltages to a modular synth, could be used to sequence other dacs/SPI interfaces.
also needs SDL

uses the analog devices ltc 2668 

I cant remember where the bitmaps came from but I didnt make them

on linux:
```
sudo apt-get install libsdl1.2-dev libsdl1.2debian
sudo apt-get install libsdl-image1.2 libsdl-image1.2-dev
make
```
on osx:
```
brew install sdl
brew install sdl_image
make
```



