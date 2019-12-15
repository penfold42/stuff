# stuff - a collection of small projects that dont justify their own repo


## ws2812_spi.c
Ever wanted to directly drive a WS2812b LED strip without using PWM or external dongles or MCUs ?
This is an example of how to (ab)use the SPI device to do it!

## ESP-udp-neo.ino
This is arduino/esp8266 project to catch UDP packets and then light up a ws2812 based strip of LEDs.

## MIDI_WS2812.ino
This is arduino/esp8266 project to listen to MIDI notes on the serial port and then light up a ws2812 based strip of LEDs.

## spi_to_2812.c
A bitbashed WS2801 to WS2811 protocol converter ideally suited to hyperion.
This does "cut through" forwarding - read 1 bit, send 1 bit so there is little lag.
This does impose a minimum SPI speed tho

## WS2801_to_2812_josh.ino
This is my original ws2801SPI to WS2812 converter.
It uses store and forward to capture a data update and then retransmit it to the LED strip
This imposes some lag and if you are too quick with subsequent SPI updates, it may drop characters as the sending process is "blocking".

## tiny2811
A simple ws2812 led tester based on an attiny13
