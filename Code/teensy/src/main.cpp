/*
   SUPER MUSCLE v.0.2.0
   ------------------------------------
   December 2020
   by David Unland david[at]unland[dot]eu
   github.com/dunland/muscle
   ------------------------------------
   ------------------------------------
*/

/* --------------------------------------------------------------------- */
/* ------------------------------- GLOBAL ------------------------------ */
/* --------------------------------------------------------------------- */
#include <Arduino.h>
#include <vector>
#include <MIDI.h>
#include <Tsunami.h>
#include <ArduinoJson.h>
#include <Globals.h>
#include <Instruments.h>
#include <Hardware.h>
#include <Score.h>
#include <Serial.h>

midi::MidiInterface<HardwareSerial> MIDI((HardwareSerial &)Serial2); // same as MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);

// define instruments:
Instrument *snare;
Instrument *hihat;
Instrument *kick;
Instrument *tom2;
Instrument *standtom;
Instrument *crash1;
// Instrument *cowbell;
Instrument *ride;
// Instrument *tom1;
// Instrument *crash2;

static std::vector<Instrument *> instruments; // all instruments go in here

Synthesizer *mKorg; // create a KORG microKorg instrument called mKorg
Synthesizer *volca; // create a KORG Volca Keys instrument called volca

// ------------------------- interrupt timers -------------------------
IntervalTimer pinMonitor; // reads pins every 1 ms

/////////////////////////// general pin reading ///////////////////////
int pinValue(Instrument *instrument)
{
  return abs(instrument->sensitivity.noiseFloor - analogRead(instrument->pin));
}
// --------------------------------------------------------------------

//////////////////////////// PRINT NORMALIZED VALUES //////////////////
void printNormalizedValues(boolean printNorm_criterion)
{
  // useful debugger for column-wise output of raw/normalised values:

  if (printNorm_criterion == true)
  {
    static unsigned long lastMillis;
    if (millis() != lastMillis)
    {
      for (auto &instrument : instruments)
      {
        // static int countsCopy[Globals::numInputs];
        //noInterrupts();
        //countsCopy[i] = counts[i];
        //interrupts();
        //Globals::print_to_console(pins[i]);
        //Globals::print_to_console(":\t");
        Globals::print_to_console(pinValue(instrument));
        Globals::print_to_console("\t");
        //Globals::print_to_console(", ");
        //Globals::print_to_console(countsCopy[i]);
      }
      Globals::println_to_console("");
    }
    lastMillis = millis();
  }
}

//////////////////// interrupt for sensor reading /////////////////////
void samplePins()
{
  // read all pins:
  for (auto &instrument : instruments)
  {
    if (pinValue(instrument) > instrument->sensitivity.threshold)
    {
      if (instrument->timing.counts < 1)
        instrument->timing.firstPinActiveTime = millis();
      instrument->timing.lastPinActiveTime = millis();
      instrument->timing.counts++;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
////////////////////////////////// SETUP //////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void setup()
{
  //---------------------- Global / Debug values ----------------------

  Globals::use_responsiveCalibration = false;
  Globals::do_print_beat_sum = false; // prints Score::beat_sum topography array
  Globals::do_print_to_console = true;
  Globals::do_print_JSON = true;

  //------------------------ initialize pins --------------------------
  pinMode(VIBR, OUTPUT);
  pinMode(FOOTSWITCH, INPUT_PULLUP);

  randomSeed(analogRead(A0) * analogRead(A19));

  //-------------------------- Communication --------------------------

  Serial.begin(115200);
  // Serial3.begin(57600); // contained in tsunami.begin()
  unsigned long wait_for_Serial = millis();

  while (!Serial) // prevents Serial flow from just stopping at some (early) point.
  {
    if (millis() > wait_for_Serial + 5000)
    {
      Globals::do_print_JSON = false;
      Globals::do_print_to_console = false;
      break;
    }
  }
  // delay(1000); // alternative to line above, if run with external power (no computer)

  MIDI.begin(MIDI_CHANNEL_OMNI);

  // setup names of elements for display on Serial Monitor:
  Score::beat_sum.tag = "v";
  Score::beat_regularity.tag = "r";

  // -------------------- Hardware initialization ---------------------
  delay(1000);              // wait for Tsunami to finish reset // redundant?
  Globals::tsunami.start(); // Tsunami startup at 57600. ATTENTION: Serial Channel is selected in Tsunami.h !!!
  delay(100);
  Globals::tsunami.stopAllTracks(); // in case Tsunami was already playing.
  Globals::tsunami.samplerateOffset(0, 0);
  Globals::tsunami.setReporting(true); // Enable track reporting from the Tsunami
  delay(100);                          // some time for Tsunami to respond with version string

  // ------------------------ INSTRUMENT SETUP ------------------------
  // instantiate external MIDI devices:
  mKorg = new Synthesizer(2);
  volca = new Synthesizer(1);
  // instantiate instruments:
  snare = new Instrument(A3, Snare);
  hihat = new Instrument(A6, Hihat);
  kick = new Instrument(A1, Kick);
  tom2 = new Instrument(A7, Tom2);
  standtom = new Instrument(A2, Standtom1);
  // cowbell = new Instrument(A3, Cowbell);
  crash1 = new Instrument(A0, Crash1);
  ride = new Instrument(A4, Ride);

  instruments = {snare, hihat, kick, tom2, standtom, crash1, ride};

  // initialize arrays:
  for (auto &instrument : instruments)
  {
    pinMode(instrument->led, OUTPUT);
    for (int j = 0; j < 8; j++)
    {
      instrument->score.read_rhythm_slot[j] = false;
      instrument->score.set_rhythm_slot[j] = false;
      instrument->topography.a_8[j] = 0;
      instrument->topography.a_16[j] = 0;
      instrument->topography.a_16[j * 2] = 0;
    }
  }

  // set instrument calibration array:

  // tom1->sensitivity.threshold = 200;
  // tom1->sensitivity.crossings = 20;
  hihat->setup_sensitivity(80, 15, 10, false);
  standtom->setup_sensitivity(200, 10, 10, false); // (2020-11-11) // 60, 20 (2020-08-27)
  tom2->setup_sensitivity(100, 9, 10, false);      // 300, 18
  kick->setup_sensitivity(200, 12, 10, false);     // (2020-11-11) // 100, 16 (2020-08-27)
  // cowbell->setup_sensitivity(80, 15, 10, false);
  crash1->setup_sensitivity(300, 2, 5, true);
  ride->setup_sensitivity(400, 2, 5, true);
  snare->setup_sensitivity(120, 10, 10, false); // (2020-11-11) // 180, 12 (2020-08-27)

  // calculate noise floor:
  for (auto &instrument : instruments)
    instrument->calculateNoiseFloor();

  // print startup information:
  Globals::println_to_console("\n..calculating noiseFloor done.");

  Globals::println_to_console("assigning effects...");

  Globals::println_to_console("-----------------------------------------------");
  Globals::println_to_console("calibration values set as follows:");
  Globals::println_to_console("instr\tthrshld\tcrosses\tnoiseFloor");
  for (Instrument *instrument : instruments)
  {
    Globals::print_to_console(Globals::DrumtypeToHumanreadable(instrument->drumtype));
    Globals::print_to_console("\t");
    Globals::print_to_console(instrument->sensitivity.threshold);
    Globals::print_to_console("\t");
    Globals::print_to_console(instrument->sensitivity.crossings);
    Globals::print_to_console("\t");
    Globals::println_to_console(instrument->sensitivity.noiseFloor);
  }
  Globals::println_to_console("-----------------------------------------------");

  // ---------------------------------- SCORE -------------------------
  // add a bass note to Score
  Score::notes.push_back(int(random(36, 48)));
  Globals::print_to_console("Score::note[0] = ");
  Globals::println_to_console(Score::notes[0]);

  // turn off all currently playing MIDI notes:
  for (int channel = 1; channel < 3; channel++)
  {
    for (int note_number = 0; note_number < 127; note_number++)
    {
      MIDI.sendNoteOff(note_number, 127, channel);
    }
  }

  // link midi synth to instruments:
  snare->midi_settings.synth = mKorg;
  kick->midi_settings.synth = mKorg;
  hihat->midi_settings.synth = mKorg;
  crash1->midi_settings.synth = mKorg;
  ride->midi_settings.synth = mKorg;
  tom2->midi_settings.synth = mKorg;
  standtom->midi_settings.synth = mKorg;
  // cowbell->midi_settings.synth = mKorg;

  // an initial midi note must be defined, otherwise there is a problem with the tidyUp function
  // snare->midi_settings.active_note = 50;
  // kick->midi_settings.active_note = 50;
  // hihat->midi_settings.active_note = 50;
  // crash1->midi_settings.active_note = 50;
  // ride->midi_settings.active_note = 50;
  // tom2->midi_settings.active_note = 50;
  // standtom->midi_settings.active_note = 50;
  // cowbell->midi_settings.active_note = 50;

  // assign startup instrument effects:
  hihat->effect = TapTempo;
  crash1->effect = Monitor;
  // cowbell->effect = Monitor;
  ride->effect = Monitor;

  // -------------------------- START TIMERS --------------------------
  pinMonitor.begin(samplePins, 1000); // sample pin every 1 millisecond

  Globals::masterClock.begin(Globals::masterClockTimer, Globals::tapInterval * 1000 * 4 / 128); // 4 beats (1 bar) with 128 divisions in microseconds; initially 120 BPM

  // Debug:
  // tsunami.trackPlayPoly(1, 0, true); // If TRUE, the track will not be subject to Tsunami's voice stealing algorithm.
  // tracknum, channel
}

/* --------------------------------------------------------------------- */
/* ------------------------------- LOOP -------------------------------- */
/* --------------------------------------------------------------------- */

void loop()
{
  Globals::tsunami.update(); // keeps variables for playing tracks etc up to date

  // ------------------------- DEBUG AREA -----------------------------
  printNormalizedValues(false);

  // --------------------- INCOMING SIGNALS FROM PIEZOS ---------------
  // (define what should happen when instruments are hit)
  for (auto &instrument : instruments)
  {
    // if (instrument->effect == PlayMidi_rawPin || instrument->effect == CC_Effect_rawPin)
    //  instrument->trigger(instrument, MIDI);

    if (instrument->stroke_detected()) // evaluates pins for activity repeatedly
    {
      // ----------------------- perform pin action -------------------
      instrument->trigger(MIDI);        // runs trigger function according to instrument's EffectType
      instrument->timing.wasHit = true; // a flag to show that the instrument was hit (for transmission via JSON)
    }
  }

  ///////////////////////////// TIMED ACTIONS /////////////////////////
  // (automatically invoke rhythm-linked actions)
  static int last_beat_pos = 0;
  static boolean toggleLED = true;
  static boolean sendMidiCopy = false;

  // get current beat position:
  noInterrupts();
  Globals::current_beat_pos = Globals::beatCount % 32; // (beatCount increases infinitely)
  sendMidiCopy = Globals::sendMidiClock;               // MIDI flag for Clock to be sent
  interrupts();

  // send MIDI-Clock on beat:
  /* MIDI Clock events are sent at a rate of 24 pulses per quarter note
     one tap beat equals one quarter note
     only one midi clock signal should be send per 24th quarter note
  */
  if (sendMidiCopy)
  {
    Serial2.write(0xF8); // MIDI-clock has to be sent 24 times per quarter note
    noInterrupts();
    Globals::sendMidiClock = false;
    interrupts();
  }

  //----------------------- DO THINGS ONCE PER 32nd-step:--------------
  //-------------------------------------------------------------------

  if (Globals::current_beat_pos != last_beat_pos)
  {
    // tidy up with previous beat position ----------------------------
    // apply topography derivations from previous beats
    // → problem: if there was any stroke at all, it was probably not on the very first run BEFORE derivation was executed

    if (Globals::do_print_JSON)
      JSON::compose_and_send_json(instruments);

    // -------------------------- 32nd-notes --------------------------

    // print millis and current beat:
    if (Globals::do_send_to_processing)
      Globals::print_to_console("m");
    Globals::print_to_console(String(millis()));
    Globals::print_to_console("\t");
    // Globals::print_to_console(Globals::current_eighth_count + 1); // if you want to print 8th-steps only
    if (Globals::do_send_to_processing)
      Globals::print_to_console("b");
    Globals::print_to_console(Globals::current_beat_pos);
    Globals::print_to_console("\t");

    // -------------------------- full notes: -------------------------
    if (Globals::current_beat_pos == 0)
    {
    }

    // ------------------------- quarter notes: -----------------------
    if (Globals::current_beat_pos % 8 == 0) // Globals::current_beat_pos holds 32 → %8 makes 4.
    {
      // Hardware::vibrate_motor(50);
      // mKorg->sendNoteOn(50, MIDI);
      // MIDI.sendNoteOn(50, 127, 2);
    }
    // Debug: play MIDI note on quarter notes
    // if (Globals::current_beat_pos % 8 == 0)
    // {
    //   MIDI.sendNoteOn(57, 127, 2);
    // }
    // else
    // {
    //   MIDI.sendNoteOff(57, 127, 2);
    // }

    // --------------------------- 8th notes: -------------------------
    if (Globals::current_beat_pos % 4 == 0)
    {
      // increase 8th note counter:
      Globals::current_eighth_count = (Globals::current_eighth_count + 1) % 8;
      toggleLED = !toggleLED;

      // blink LED rhythmically:
      digitalWrite(LED_BUILTIN, toggleLED);
    }

    // --------------------------- 16th notes: ------------------------
    if (Globals::current_beat_pos % 2 == 0)
    {
      // increase 16th note counter:
      Globals::current_16th_count = (Globals::current_16th_count + 1) % 16;
    }

    // ----------------------------- draw play log to console

    // Globals::print_to_console(Globals::current_beat_pos / 4);
    // Globals::print_to_console("\t");
    // Globals::print_to_console(Globals::current_eighth_count);
    for (auto &instrument : instruments)
    {
      Globals::print_to_console(instrument->output_string);
      instrument->output_string = "\t";
    }

    // sum up all topographies of all instruments:
    Score::beat_sum.reset();
    for (auto &instrument : instruments)
    {
      if (instrument->drumtype != Ride && instrument->drumtype != Crash1 && instrument->drumtype != Crash2) // cymbals have too many counts
        Score::beat_sum.add(&instrument->topography);
    }
    Score::beat_sum.smoothen_dataArray();

    Globals::print_to_console("avg: ");
    Globals::print_to_console(Score::beat_sum.average_smooth);
    Globals::print_to_console("/");
    Globals::print_to_console(Score::beat_sum.activation_thresh);
    Globals::print_to_console("\tstep:");
    Globals::println_to_console(Score::step);

    Score::beat_regularity.derive_from(&Score::beat_sum); // TODO: also do this for all instruments

    // print topo arrays:
    if (Globals::do_print_beat_sum)
    {
      // for (auto &instrument : instruments)
      // Globals::printTopoArray(&instrument->topography);
      Globals::printTopoArray(&Score::beat_sum); // print volume layer
    }
    // Globals::printTopoArray(&Score::beat_regularity);

    // perform timed pin actions according to current beat:
    for (auto &instrument : instruments)
    {
      instrument->perform(instruments, MIDI);
    }

    //////////////////////////////// SCORE ////////////////////////////
    ///////////////////////////////////////////////////////////////////

    // step proceeds if footswitch is pressed (in mode RESET_AND_PROCEED_SCORE) when regularity is high enough

    // THIS SONG IS COMPOSED FOR microKORG A.63
    // SCORE, stepwise:

    static int rhythmic_iterator;
    static std::vector<int> locrian_mode = {Score::notes[0] + 1, Score::notes[0] + 3, Score::notes[0] + 5, Score::notes[0] + 6, Score::notes[0] + 8, Score::notes[0] + 11};
    static int note_idx = -1;

    switch (Score::step)
    {

    case 0: // cymbalsEffectsChange
    {
      if (Score::setup)
      {
        // Cymbals → random CC (mKorg)
        // drums → playMidi (Volca)

        // assign random-cc-effect to cymbals:
        crash1->setup_midi(None, mKorg);
        crash1->set_effect(Random_CC_Effect);
        ride->setup_midi(None, mKorg);
        ride->set_effect(Random_CC_Effect);

        // set list of notes for kick, snare, tom, standtom
        kick->set_notes(locrian_mode);
        snare->set_notes(locrian_mode);
        tom2->set_notes(locrian_mode);
        standtom->set_notes(locrian_mode);

        // proceed in note lists:
        note_idx = (note_idx + 1) % locrian_mode.size();
        Score::note_idx = (Score::note_idx + 1) % Score::notes.size();

        // set locrian mode
        Score::set_notes({locrian_mode[0], locrian_mode[1], locrian_mode[2]});

        // start bass note:
        Score::playSingleNote(mKorg, MIDI);

        // leave setup:
        Score::setup = false;
      }
    }
    break;

    case 1: // drumsPlayNotes_cymbalNoteIteration

      if (Score::setup)
      {
        // add locrian mode
        Score::set_notes({locrian_mode[0], locrian_mode[1], locrian_mode[2]});

        // set crash1 to change main note:
        crash1->setup_midi(None, mKorg);
        crash1->set_effect(MainNoteIteration);

        // drums play on Volca:
        kick->setup_midi(None, volca);
        snare->setup_midi(None, volca);
        tom2->setup_midi(None, volca);
        standtom->setup_midi(None, volca);

        kick->midi_settings.active_note = kick->midi_settings.notes[note_idx];
        kick->set_effect(PlayMidi);

        snare->midi_settings.active_note = snare->midi_settings.notes[note_idx] + 12;
        snare->set_effect(PlayMidi);

        tom2->midi_settings.active_note = tom2->midi_settings.notes[(note_idx + 1) % tom2->midi_settings.notes.size()];
        tom2->set_effect(PlayMidi);

        standtom->midi_settings.active_note = standtom->midi_settings.notes[(note_idx + 2) % standtom->midi_settings.notes.size()];
        standtom->set_effect(PlayMidi);

        // define interval when to change notes:
        // rhythmic_iterator = int(random(32));
        // Globals::print_to_console("rhythmic_iterator = ");
        // Globals::println_to_console(rhythmic_iterator);

        // set locrian mode:
        Score::set_notes({locrian_mode[0], locrian_mode[1], locrian_mode[2]});

        // proceed in note lists:
        note_idx = (note_idx + 1) % locrian_mode.size();
        Score::note_idx = (Score::note_idx + 1) % Score::notes.size();

        // start bass note:
        Score::playSingleNote(mKorg, MIDI);

        // leave setup:
        Score::setup = false;
      }

      // Score::playRhythmicNotes(mKorg, MIDI, rhythmic_iterator);

      break;

    case 2: // automatic bass notes

      // main notes: locrian 1, 2, 3, 3 (intervals 0, 3, 5, 5)

      if (Score::setup)
      {
        // drums play on Volca:
        kick->setup_midi(None, volca);
        snare->setup_midi(None, volca);
        tom2->setup_midi(None, volca);
        standtom->setup_midi(None, volca);

        kick->midi_settings.active_note = kick->midi_settings.notes[note_idx];
        kick->set_effect(PlayMidi);

        snare->midi_settings.active_note = snare->midi_settings.notes[note_idx] + 12;
        snare->set_effect(PlayMidi);

        tom2->midi_settings.active_note = tom2->midi_settings.notes[(note_idx + 1) % tom2->midi_settings.notes.size()];
        tom2->set_effect(PlayMidi);

        standtom->midi_settings.active_note = standtom->midi_settings.notes[(note_idx + 2) % standtom->midi_settings.notes.size()];
        standtom->set_effect(PlayMidi);

        // set locrian mode (last note twice!)
        Score::set_notes({locrian_mode[0], locrian_mode[1], locrian_mode[2], locrian_mode[2]});

        // proceed in note lists:
        note_idx = (note_idx + 1) % locrian_mode.size();
        Score::note_idx = (Score::note_idx + 1) % Score::notes.size();

        // leave setup:
        Score::setup = false;
      }

      Score::playNoteAtPosition(mKorg, MIDI, 0);

      break;

      //   case 0: // init state
      //     // setup score note seed:
      //     Globals::print_to_console("Score::notes[0] = ");
      //     Globals::println_to_console(Score::notes[0]);
      //     // set start values for microKORG:

      //     mKorg->sendControlChange(Cutoff, 50, MIDI); // sets cc_value and sends MIDI-ControlChange
      //     mKorg->sendControlChange(Mix_Level_1, 0, MIDI);
      //     mKorg->sendControlChange(Mix_Level_2, 127, MIDI);
      //     mKorg->sendControlChange(Osc2_tune, 0, MIDI);
      //     mKorg->sendControlChange(Osc2_semitone, 39, MIDI);
      //     mKorg->sendControlChange(Cutoff, 50, MIDI);
      //     mKorg->sendControlChange(Resonance, 13, MIDI);
      //     mKorg->sendControlChange(Amplevel, 0, MIDI);

      //     Score::step = 1;

      //     break;

      //   case 1:             // fade in the synth's amplitude
      //     if (Score::setup) // score setup is run once and reset when next score step is activated.
      //     {
      //       // assign effects to instruments:
      //       // the hihat will change the allocated (AmpLevel) value on the synth, whenever hit:
      //       hihat->effect = Change_CC;
      //       hihat->setup_midi(Amplevel, mKorg, 127, 0, 0.65, 0);

      //       // these instruments do not play a role here. just print out what they do:
      //       snare->effect = Monitor;
      //       kick->effect = Monitor;
      //       tom2->effect = Monitor;
      //       ride->effect = Monitor;
      //       crash1->effect = Monitor;
      //       standtom->effect = Monitor;

      //       Score::playSingleNote(mKorg, MIDI); // start playing a bass note on synth
      //       Score::setup = false;                   // leave setup section
      //     }

      //     Globals::print_to_console("amplevel_val = ");
      //     Globals::println_to_console(hihat->midi_settings.cc_val);

      //     if (hihat->midi_settings.cc_val >= 127)
      //     {
      //       Score::beat_sum.activation_thresh = 0; // score step is ready
      //     }
      //     break;

      //   case 2:
      //     /* kick -> play note
      //        snare -> play note
      //     */

      //     if (Score::setup)
      //     {
      //       // assign effects to instruments:
      //       kick->set_effect(PlayMidi);
      //       kick->setup_midi(None, volca, 127, 0, 1, 0.1);
      //       kick->midi_settings.notes.push_back(Score::notes[0] + 12 + 7);
      //       kick->midi_settings.active_note = kick->midi_settings.notes[0];

      //       Globals::print_to_console("note for kick is: ");
      //       Globals::print_to_console(kick->midi_settings.notes[0]);
      //       Globals::print_to_console(" active note:");
      //       Globals::println_to_console(kick->midi_settings.active_note);

      //       snare->set_effect(PlayMidi);
      //       snare->setup_midi(None, volca, 127, 0, 1, 0.1);
      //       snare->midi_settings.notes.push_back(Score::notes[0] + 24);
      //       snare->midi_settings.active_note = snare->midi_settings.notes[0];

      //       Globals::print_to_console("note for snare is: ");
      //       Globals::print_to_console(snare->midi_settings.notes[0]);
      //       Globals::print_to_console(" active note:");
      //       Globals::println_to_console(snare->midi_settings.active_note);
      //       Score::setup = false;
      //     }
      //     break;

      // break;

      //   case 4:

      //     // beat_sum -> increase cutoff
      //     // beat_sum -> fade in OSC1
      //     // snare, kick, ride, crash = FX
      //     static float step_factor;

      //     if (Score::setup)
      //     {
      //       Score::beat_sum.activation_thresh = 10;
      //       step_factor = 127 / Score::beat_sum.activation_thresh;

      //       hihat->effect = TapTempo;
      //       standtom->effect = Monitor;
      //       snare->effect = Change_CC;
      //       kick->effect = Change_CC;
      //       ride->effect = Change_CC;
      //       crash1->effect = Change_CC;

      //       // midi channels (do not use any Type twice → smaller/bigger will be ignored..)
      //       snare->setup_midi(Osc2_tune, mKorg, 127, 13, 15, -0.1);
      //       kick->setup_midi(Amplevel, mKorg, 127, 80, -35, 0.1);
      //       ride->setup_midi(Resonance, mKorg, 127, 0, 0.4, -0.1);      // TODO: implement oscillation possibility
      //       crash1->setup_midi(Patch_3_Depth, mKorg, 127, 64, 2, -0.1); // Patch 3 is Pitch on A.63; extends -63-0-63 → 0-64-127

      //       Score::setup = false;
      //     }

      //     // change cutoff with overall beat_sum until at max
      //     static int cutoff_val = 50;

      //     cutoff_val = max(50, (min(127, Score::beat_sum.average_smooth * step_factor)));
      //     mKorg->sendControlChange(Cutoff, cutoff_val, MIDI);

      //     // fade in Osc1 slowly
      //     static int osc1_level;
      //     osc1_level = min(127, Score::beat_sum.average_smooth * 4);
      //     mKorg->sendControlChange(Mix_Level_1, osc1_level, MIDI);

      //     break;

      //   case 5: // increase osc2_tune with snare and osc2_semitone with beat_sum
      //     static float osc2_semitone_val;

      //     // Score::set_ramp(...);

      //     if (Score::setup)
      //     {
      //       Score::beat_sum.activation_thresh = 15;
      //       snare->effect = Change_CC;
      //       snare->setup_midi(Osc2_tune, mKorg, 64, 0, 1, 0);      // does not decrease
      //       kick->setup_midi(DelayDepth, mKorg, 127, 0, 50, -0.1); // TODO: implement oscillation possibility
      //       ride->setup_midi(Cutoff, mKorg, 127, 13, -0.7, 0.1);   // TODO: implement oscillation possibility
      //       Score::setup = false;
      //     }

      //     // increase osc2_semitone with beat_sum until at 0
      //     osc2_semitone_val = max(0, (min(64, Score::beat_sum.average_smooth * (127 / 15))));
      //     osc2_semitone_val = max(0, min(127, osc2_semitone_val));

      //     Globals::print_to_console("\n -- Osc2-tune = ");
      //     Globals::print_to_console(snare->midi_settings.cc_val);
      //     Globals::println_to_console(" --");

      //     break;

      //   case 6: // adds new bass note and switches bass once per bar

      //     if (Score::setup)
      //     {
      //       Score::beat_sum.activation_thresh = 15;

      //       // cutoff on tom2:
      //       tom2->effect = Change_CC;
      //       tom2->setup_midi(Cutoff, mKorg, 127, 20, 30, -0.1);
      //       ride->effect = Monitor;
      //       snare->effect = Monitor;

      //       mKorg->sendControlChange(Resonance, 31, MIDI);
      //       mKorg->sendControlChange(Cutoff, 28, MIDI);
      //       mKorg->sendControlChange(Osc2_tune, 0, MIDI);
      //       Score::add_bassNote(Score::notes[0] + int(random(6)));
      //       // Score::note_change_pos = int(random(8, 16)); // change at a rate between quarter and half notes
      //       Score::setup = false;
      //     }

      //     Score::playRhythmicNotes(mKorg, MIDI);
      //     break;

      //   case 7: // play melodies on snare, tom2, standtom
      //     if (Score::setup)
      //     {
      //       Score::add_bassNote(Score::notes[0] + int(random(6)));

      //       snare->effect = PlayMidi;
      //       snare->midi_settings.active_note = Score::notes[0] + 24 + 7;
      //       tom2->effect = PlayMidi;
      //       tom2->midi_settings.active_note = Score::notes[0] + 24 + 3;
      //       standtom->effect = PlayMidi;
      //       standtom->midi_settings.active_note = Score::notes[0] + 12 + 4;

      //       kick->effect = Monitor;
      //       Score::setup = false;
      //     }

      //     // static int random_note_change = int(random(32));
      //     Score::playRhythmicNotes(mKorg, MIDI);
      //     break;

      //     // case 7: // swell effect with notes!
      //     // break;

    default: // go back to beginning...
      Score::step = 0;
      break;
    }

    // vibrate if new score is ready:
    if (Score::beat_sum.ready())
    {
      digitalWrite(VIBR, HIGH);
      Globals::println_to_console("ready to go to next score step! hit footswitch!");
    }
    else
      digitalWrite(VIBR, LOW);

    //----------------------- SCORE END -------------------------------

    for (auto &instrument : instruments)
    {
      instrument->timing.wasHit = false;
    }

  } // end of (32nd-step) TIMED ACTIONS
  // ------------------------------------------------------------------

  ///////////////////////////// tidy up ///////////////////////////////
  last_beat_pos = Globals::current_beat_pos;
  Globals::last_eighth_count = Globals::current_eighth_count;
  Globals::last_16th_count = Globals::current_16th_count;

  // Hardware:
  Hardware::checkFootSwitch(instruments); // check step of footswitch
  // Hardware::request_motor_deactivation(); // turn off vibration and MIDI notes

  // tidying up what's left from performing functions..
  for (auto &instrument : instruments)
    instrument->tidyUp(MIDI);

  // MIDI.sendControlChange(50, 100, 2);
}
// --------------------------------------------------------------------
