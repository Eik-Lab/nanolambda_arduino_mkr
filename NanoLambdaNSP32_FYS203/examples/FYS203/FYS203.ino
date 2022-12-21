/*
 * Copyright (C) 2019 nanoLambda, Inc.
 * Copyright (C) 2022 Eik Lab, NMBU
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 */

/*
 * ---------------------------------------------------------
 *               NSP32      Arduino       Arduino   Arduino   
 *  UART                    Uno/101       Mega      MKR Zero 
 * Signal        Pin          Pin           Pin       Pin     
 * -----------------------------------------------------------
 * Wakeup/Reset  RST          8             49        0      
 * UART RX       TX           10            53        13
 * UART TX       RX           13 / ICSP-3   52        14
 * Ready         Ready        2             21        1
 * 
 */

#include <ArduinoAdaptor.h>
#include <NSP32.h>
#include <SPI.h>
#include <SD.h>

const int chipSelect = SDCARD_SS_PIN;
#define FILENAME "spectrum.csv"

using namespace NanoLambdaNSP32;

// ***************** NSP32m model parameters *****************

#define FIRST_WAVE_LEN 600
#define NUMBER_OF_WAVE_LEN 81


/***********************************************************************************
 * modify this section to fit your need                                            *
 ***********************************************************************************/

	const unsigned int PinRst	= 0;  	// pin Reset
	const unsigned int PinReady	= 1;  	// pin Ready

  // button  
  const unsigned int ButtonPin = 6;
  const unsigned int ButtonVal = 7;

/***********************************************************************************/

ArduinoAdaptor adaptor(PinRst);				// master MCU adaptor
// SPI is not tested on MKR boards!
NSP32 nsp32(&adaptor, NSP32::ChannelUart);	// NSP32 (using Uart channel)

void setup() 
{
  if (!SD.begin(chipSelect)) {
    // don't do anything more:
    while (1);
  }

	// initialize "ready trigger" pin for accepting external interrupt (falling edge trigger)
	pinMode(PinReady, INPUT_PULLUP);     // use pull-up
	attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING);  // enable interrupt for falling edge

  // button
  pinMode(ButtonPin, OUTPUT);
  digitalWrite(ButtonPin, LOW);
  pinMode(ButtonVal, INPUT_PULLUP);
	
	// initialize serial port for "Serial Monitor"
	Serial.begin(115200);

  delay(2000);
  Serial.println("Software started!");

  File dataFile = SD.open(FILENAME, FILE_WRITE);
  String wavestr = "";
  if (dataFile)
  {
    for(int wl = 0; wl < NUMBER_OF_WAVE_LEN; wl++)
    {
      int wavelength = FIRST_WAVE_LEN + wl*5;
      wavestr += String(wavelength);
      wavestr += ";";
    }
    dataFile.println(wavestr);
    dataFile.close();
  }
	
	// initialize NSP32
	nsp32.Init();

	// =============== standby ===============
	nsp32.Standby(0);
}

void loop() 
{
  if(digitalRead(ButtonVal) == LOW) // change to "if(true)" to remove button functionality
  {
    // Wake up
    nsp32.Wakeup();

    /* Not needed, since we know the wavelengths...
    // =============== get wavelength ===============
    nsp32.GetWavelength(0);

    WavelengthInfo infoW;
    nsp32.ExtractWavelengthInfo(&infoW);	// now we have all wavelength info in infoW, we can use e.g. "infoW.Wavelength" to access the wavelength data array
    */

    // =============== spectrum acquisition ===============
    nsp32.AcqSpectrum(0, 100, 10, false);		// integration time = 100; frame avg num = 10; disable AE

    // "AcqSpectrum" command takes longer time to execute, the return packet is not immediately available
    // when the acquisition is done, a "ready trigger" will fire, and nsp32.GetReturnPacketSize() will be > 0
    while(nsp32.GetReturnPacketSize() <= 0)
    {
      // TODO: can go to sleep, and wakeup when "ready trigger" interrupt occurs
      
      nsp32.UpdateStatus();	// call UpdateStatus() to check async result
    }

    SpectrumInfo infoS;
    nsp32.ExtractSpectrumInfo(&infoS);		// now we have all spectrum info in infoW, we can use e.g. "infoS.Spectrum" to access the spectrum data array

    File dataFile = SD.open(FILENAME, FILE_WRITE);
    String spectra = " ";
    if (dataFile)
    {
      for(int wl = 0; wl < NUMBER_OF_WAVE_LEN; wl++)
      {
        int wavelength = FIRST_WAVE_LEN + wl*5; //modify this based on the NSP32 you have, starts at 600
        Serial.print("Wavelength, spectre: ");
        Serial.print(wavelength);
        Serial.print(", ");
        Serial.println(infoS.Spectrum[wl]);

        spectra += String(infoS.Spectrum[wl]);
        spectra += "; ";
      }
      dataFile.println(spectra);
      dataFile.close();
    }


    Serial.println(" ");
    delay(1500); // added to remove "double click" on button. Can be used to sample periodicly when button functionality is turned off
  }
}

void PinReadyTriggerISR()
{
	// make sure to call this function when receiving a ready trigger from NSP32
	nsp32.OnPinReadyTriggered();
}
