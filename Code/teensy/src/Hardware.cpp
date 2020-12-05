#include <Hardware.h>
#include <Score.h>
#include <Instruments.h>

////////////////////////////////// FOOT SWITCH ////////////////////////
///////////////////////////////////////////////////////////////////////

void Hardware::footswitch_pressed(std::vector<Instrument *> instruments)
{
  switch (FOOTSWITCH_MODE)
  {
  case (LOG_BEATS):
    // set pinMode of all instruments to 3 (record what is being played)
    for (auto &instrument : instruments)
    {
      instrument->lastEffect = instrument->effect;
      instrument->effect = FootSwitchLooper; // TODO: not for Cowbell?
      for (int j = 0; j < 8; j++)
        instrument->score.set_rhythm_slot[j] = false; // reset entire record
    }
    break;

  case (HOLD_CC): // prevents swell_vals to be changed in swell_beat()
    Globals::footswitch_is_pressed = true;
    break;

  case (RESET_TOPO): // resets beat_topography (for all instruments)
    for (auto &instrument : instruments)
    {
      // reset 8th-note-topography:
      for (int j = 0; j < 8; j++)
      {
        instrument->topography.a_8[j] = 0;
      }

      // reset 16th-note-topography:
      for (int j = 0; j < 16; j++)
      {
        instrument->topography.a_16[j] = 0;
      }
    }
    break;

  case (RESET_AND_PROCEED_SCORE):
    if (Score::beat_sum.average_smooth >= Score::beat_sum.activation_thresh) // score proceed criterion reached
    {
      Score::proceed_to_next_step(instruments);
    }

    else // not enough strokes to proceed yet.
    {
      Globals::print_to_console("regularity too low to proceed.. is at ");
      Globals::println_to_console(Score::beat_sum.average_smooth);
    }

    // either way, shuffle instruments with Random_CC_Effect:
    for (auto &instrument : instruments)
    {
      if (instrument->effect == Random_CC_Effect)
        instrument->score.ready_to_shuffle = true;
    }

    break;

  default:
    Globals::println_to_console("Footswitch mode not defined!");
    break;
  }
}

void Hardware::footswitch_released(std::vector<Instrument *> instruments)
{
  switch (FOOTSWITCH_MODE)
  {
  case (LOG_BEATS):
    for (auto &instrument : instruments)
      instrument->effect = instrument->lastEffect;
    break;

  case (HOLD_CC):
    Globals::footswitch_is_pressed = false;
    break;

  default:
    break;
  }
}

void Hardware::checkFootSwitch(std::vector<Instrument *> instruments)
{

  static int switch_state;
  static int last_switch_state = HIGH;
  static unsigned long last_switch_toggle = 1000; // some pre-delay to prevent initial misdetection

  switch_state = digitalRead(FOOTSWITCH);
  if (switch_state != last_switch_state && millis() > last_switch_toggle + 20)
  {
    if (switch_state == LOW)
    {
      footswitch_pressed(instruments);
      Globals::println_to_console("Footswitch pressed.");
    }
    else
    {
      footswitch_released(instruments);
      Globals::println_to_console("Footswitch released.");
    }
    last_switch_state = switch_state;
    last_switch_toggle = millis();
  }
}

////////////////////////////// VIBRATION MOTOR ////////////////////////
///////////////////////////////////////////////////////////////////////
unsigned long Hardware::motor_vibration_begin = 0;
int Hardware::motor_vibration_duration = 0;

void Hardware::vibrate_motor(unsigned long vibration_duration_input)
{
  motor_vibration_begin = millis();
  motor_vibration_duration = vibration_duration_input;
  digitalWrite(VIBR, HIGH);
}

void Hardware::request_motor_deactivation() // turn off vibration and MIDI notes
{
  if (millis() > motor_vibration_begin + motor_vibration_duration)
    digitalWrite(VIBR, LOW);
}

//////////////////////// SYNTHESIZER CLASS ////////////////////////////
///////////////////////////////////////////////////////////////////////

// sets cc_value (for JSON communication) and sends MIDI-ControlChange:
void Synthesizer::sendControlChange(CC_Type cc_type, int val, midi::MidiInterface<HardwareSerial> MIDI)
{
  switch (cc_type)
  {
  case Osc2_semitone:
    osc2_semitone = val;
    break;

  case Osc2_tune:
    osc2_tune = val;
    break;

  case Mix_Level_1:
    mix_level_1 = val;
    break;

  case Mix_Level_2:
    mix_level_2 = val;
    break;

  case Patch_1_Depth:
    patch1_depth = val;
    break;

  case Patch_3_Depth:
    patch3_depth = val;
    break;

  case Cutoff:
    cutoff = val;
    break;

  case Resonance:
    resonance = val;
    break;

  case Amplevel:
    amplevel = val;
    break;

  case Attack:
    attack = val;
    break;

  case Sustain:
    sustain = val;
    break;

  case Release:
    release = val;
    break;

  case DelayTime:
    delaytime = val;
    break;

  case DelayDepth:
    delaydepth = val;
    break;

  default:
    break;
  }

  if (cc_type < 0)
  {
    Globals::println_to_console("could not send MIDI CC Command: CC_Type not defined.");
  }
  else
  {
    MIDI.sendControlChange(int(cc_type), val, midi_channel);
  }
}

void Synthesizer::sendControlChange(int cc_type, int val, midi::MidiInterface<HardwareSerial> MIDI)
{

  // TODO: complete this list. May lead to incomplete monitoring otherwise..
  switch (cc_type)
  {
  case Osc2_semitone:
    osc2_semitone = val;
    break;

  case Osc2_tune:
    osc2_tune = val;
    break;

  case Mix_Level_1:
    mix_level_1 = val;
    break;

  case Mix_Level_2:
    mix_level_2 = val;
    break;

  case Patch_1_Depth:
    patch1_depth = val;
    break;

  case Patch_3_Depth:
    patch3_depth = val;
    break;

  case Cutoff:
    cutoff = val;
    break;

  case Resonance:
    resonance = val;
    break;

  case Amplevel:
    amplevel = val;
    break;

  case Attack:
    attack = val;
    break;

  case Sustain:
    sustain = val;
    break;

  case Release:
    release = val;
    break;

  case DelayTime:
    delaytime = val;
    break;

  case DelayDepth:
    delaydepth = val;
    break;

  default:
    break;
  }

  if (cc_type < 0)
  {
    Globals::println_to_console("could not send MIDI CC Command: CC_Type not defined.");
  }
  else
  {
    MIDI.sendControlChange(int(cc_type), val, midi_channel);
  }
}

void Synthesizer::sendNoteOn(int note, midi::MidiInterface<HardwareSerial> MIDI)
{
  notes[note] = true; // remember that note is turned on
  MIDI.sendNoteOn(note, 127, midi_channel);
}

void Synthesizer::sendNoteOff(int note, midi::MidiInterface<HardwareSerial> MIDI)
{
  notes[note] = false; // remember that note is turned off
  MIDI.sendNoteOff(note, 127, midi_channel);
}