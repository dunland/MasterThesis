#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Tsunami.h> // in cpp
// #include <MIDI.h> // in cpp

//MIDI_CREATE_DEFAULT_INSTANCE();
// MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI); // for Serial-specific usage

#define VIBR 0
#define FOOTSWITCH 2

// instrument naming for human-readable console outputs:
enum DrumType
{
  Snare,
  Hihat,
  Kick,
  Tom1,
  Tom2,
  Standtom1,
  Cowbell,
  Standtom2,
  Ride,
  Crash1,
  Crash2
};

enum EffectsType
{
  PlayMidi = 0,
  Monitor = 1,
  ToggleRhythmSlot = 2,
  FootSwitchLooper = 3,
  TapTempo = 4,
  Swell = 5,
  TsunamiLink = 6,
  CymbalSwell = 7,
  TopographyLog = 8
};

class Globals
{
public:
  static const uint8_t numInputs = 7;

  static String DrumtypeToHumanreadable(DrumType type)
  {
    switch (type)
    {
    case Snare:
      return "Snare";
    case Hihat:
      return "Hihat";
    case Kick:
      return "Kick";
    case Tom1:
      return "Tom1";
    case Tom2:
      return "Tom2";
    case Standtom1:
      return "Standtom1";
    case Standtom2:
      return "Standtom2";
    case Ride:
      return "Ride";
    case Crash1:
      return "Crash1";
    case Crash2:
      return "Crash2";
    case Cowbell:
      return "Cowbell";
    }
    return "";
  }

  // static boolean printStrokes;
  static String output_string[numInputs];

  static void setInstrumentPrintString(DrumType drum, EffectsType effect);
};

struct TOPOGRAPHY
{
  int a_8[8];   // 8-bit array for comparison with 8-bit-length sound files
  int a_16[16]; // 16-bit array for abstractions like beat regularity etc
  int threshold;
  int average_smooth;
};

#endif