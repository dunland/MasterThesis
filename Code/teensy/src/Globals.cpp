#include <Globals.h>
#include <Tsunami.h>
#include <ArduinoJson.h>

bool TOPOGRAPHY::ready()
{
	return (average_smooth > activation_thresh);
}

boolean Globals::printStrokes = true;
boolean Globals::use_responsiveCalibration = false;
boolean Globals::do_print_to_console = true;
boolean Globals::do_send_to_processing = false;
boolean Globals::do_print_beat_sum = false; // prints Score::beat_sum topography array
boolean Globals::do_print_JSON = true;		// will be deactivated if no USB answer for 5 seconds

IntervalTimer Globals::masterClock; // 1 bar

Tsunami Globals::tsunami;

float Globals::track_bpm[256] =
	{
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 100, 1, 1, 1, 1, 1,
		1, 1, 90, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 73, 1, 1, 1,
		100, 1, 1, 1, 200, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 41, 1, 1,
		103, 1, 1, 1, 1, 1, 1, 93, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 100, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 78, 1, 100, 100, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 100, 1, 1, 1, 1,
		1, 1, 1, 1, 100, 60};

// int swell_val[numInputs]; // this should be done in the swell section, but is needed in print section already... :/

int Globals::current_beat_pos = 0;	   // always stores the current position in the beat
int Globals::current_eighth_count = 0; // overflows at current_beat_pos % 8
int Globals::current_16th_count = 0;   // overflows at current_beat_pos % 2
int Globals::last_eighth_count = 0;	   // stores last eightNoteCount for comparison
int Globals::last_16th_count = 0;	   // stores last eightNoteCount for comparison

int Globals::tapInterval = 500; // 0.5 s per beat for 120 BPM
int Globals::current_BPM = 120;

boolean Globals::footswitch_is_pressed = false;

// ----------------------------- timer counter ---------------------------------
volatile unsigned long Globals::masterClockCount = 0; // 4*32 = 128 masterClockCount per cycle
volatile unsigned long Globals::beatCount = 0;
int Globals::next_beatCount = 0; // will be reset when timer restarts
volatile boolean Globals::sendMidiClock = false;

void Globals::masterClockTimer()
{
	/*
    timing              fraction          bar @ 120 BPM    bar @ 180 BPM
    1 bar             = 1               = 2 s       | 1.3 s
    1 beatCount       = 1/32 bar        = 62.5 ms   | 41.7 ms
    stroke precision  = 1/4 beatCount   = 15.625 ms | 10.4166 ms


    |     .-.
    |    /   \         .-.
    |   /     \       /   \       .-.     .-.     _   _
    +--/-------\-----/-----\-----/---\---/---\---/-\-/-\/\/---
    | /         \   /       \   /     '-'     '-'
    |/           '-'         '-'
    |--------2 oscil---------|
    |------snare = 10 ms-----|

  */

	masterClockCount++; // will rise infinitely

	// ------------ 1/32 increase in 4 full precisionCounts -----------
	if (masterClockCount % 4 == 0)
	{
		beatCount++; // will rise infinitely
	}

	// evaluate current position of beat in bar for stroke precision
	// 2020-09-07: this doesn't help and it's also not working...
	// if ((masterClockCount % 4) >= next_beatCount - 2)
	// {
	//   currentStep = next_beatCount;
	//   next_beatCount += 4;
	// }

	// prepare MIDI clock:
	sendMidiClock = (((masterClockCount % 4) / 3) % 4 == 0);
}
// ---------------------------------------------------------------------------

// ----------------------------- TOPOGRAPHIES -------------------------
void TOPOGRAPHY::reset()
{
	a_16 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

void TOPOGRAPHY::add(TOPOGRAPHY *to_add)
{
	// sum up all topographies of all instruments:
	for (int idx = 0; idx < 16; idx++) // each slot
	{
		a_16[idx] += to_add->a_16[idx];
	}
}

// TODO:
//makeTopo();
// works like this:
// instruments[0]->smoothen_dataArray(instruments[0]);
// LOTS OF PSEUDOCODE HERE:

//    beat_topography_16.smoothen(); // → gives you beat_topography.average_smoothened_height
//
//    if (beat_topography_16.average_smoothened_height >= beat_topography.threshold)
//    {
//      // create next abstraction layer
//      int[] beat_regularity = new int[16];
//
//      // update abstraction layer:
//      for (int i = 0; i < 16; i++)
//        beat_regularity[i] = beat_topography_16[i];
//
//      // get average height again:
//      beat_regularity.smoothen();
//    }
//
//    if (beat_regularity.amplitude >= beat_regularity.threshold)
//    {
//      // you are playing super regularly. that's awesome. now activate next element in score
//      score_next_element_ready = true;
//    }
//
//    // if regularity is held for certain time (threshold reached):
//    read_to_break
//
//    when THEN regularity expectations not fulfilled:
//    score: go to next element
//
//    // elsewhere:
//    if (hit_certain_trigger)
//    {
//
//      if (score_next_element_ready)
//      {
//        /* do whatever is up next in the score */
//      }
//    }

/* Score could look like this:
      intro----------part 1--...-----outro--------
      1     2     3     4    ...     20    21     step
      ++    ++    ++                              element_FX
                  ++    ++   ...     ++    ++     element_notes
                        ++           ++           element_fieldRecordings

      cool thing: create score dynamically according to how I play
    */

// PSEUDOCODE END
// -->
void TOPOGRAPHY::derive_from(TOPOGRAPHY *original)
{
	if (original->average_smooth < original->activation_thresh)
		a_16[Globals::current_16th_count] = original->a_16[Globals::current_16th_count]; // sync both arrays

	else if (original->average_smooth >= original->activation_thresh) // only execute if threshold of original topography
	{

		// always check for previous beat position:
		int beat_position;
		if (Globals::current_16th_count > 0)
			beat_position = Globals::current_16th_count - 1;
		else
			beat_position = 15;

		// reset flags (for console output)
		flag_empty_increased[beat_position] = false;
		flag_empty_played[beat_position] = false;
		flag_occupied_increased[beat_position] = false;
		flag_occupied_missed[beat_position] = false;

		// CASES:

		// empty slot repeatedly not played: increase:
		if (original->a_16[beat_position] == 0 && a_16[beat_position] == 0)
		{
			regularity++;
			flag_empty_increased[beat_position] = true;
		}

		// occupied slot repeatedly played: increase
		else if (original->a_16[beat_position] > 0 && original->a_16[beat_position] > a_16[beat_position])
		{
			regularity++;
			flag_occupied_increased[beat_position] = true;
		}

		// occupied slot NOT played : decrease
		else if (original->a_16[beat_position] > 0 && original->a_16[beat_position] <= a_16[beat_position])
		{
			regularity--;
			regularity = max(0, regularity);
			flag_occupied_missed[beat_position] = true;
		}

		// empty slot PLAYED: decrease
		else if (a_16[beat_position] == 0 && original->a_16[beat_position] > 0)
		{
			regularity--;
			regularity = max(0, regularity);
			flag_empty_played[beat_position] = true;
		}

		// copy contents:
		a_16[beat_position] = original->a_16[beat_position];
	}
}

// ---------------- smoothen 16-bit array using struct ----------------
void TOPOGRAPHY::smoothen_dataArray()
{
	/* input an array of size 16
1. count entries and create squared sum of each entry
2. calculate (squared) fraction of total for each entry
3. get highest of these fractions
4. get ratio of highest fraction to other and reset values if ratio > threshold
->
*/

	int len = a_16.size(); // TODO: use dynamic vector topography.a instead
	int entries = 0;
	int squared_sum = 0;
	regular_sum = 0;

	// count entries and create squared sum:
	for (int j = 0; j < len; j++)
	{
		if (a_16[j] > 0)
		{
			entries++;
			squared_sum += a_16[j] * a_16[j];
			regular_sum += a_16[j];
		}
	}

	regular_sum = regular_sum / entries;

	// calculate site-specific (squared) fractions of total:
	float squared_frac[len];
	for (int j = 0; j < len; j++)
		squared_frac[j] =
			float(a_16[j]) / float(squared_sum);

	// get highest frac:
	float highest_squared_frac = 0;
	for (int j = 0; j < len; j++)
		highest_squared_frac = (squared_frac[j] > highest_squared_frac) ? squared_frac[j] : highest_squared_frac;

	// SMOOTHEN ARRAY / get "topography height":
	// divide highest with other entries and reset entries if ratio > threshold:
	flag_entry_dismissed = false;
	for (int j = 0; j < len; j++)
		if (squared_frac[j] > 0)
			if (highest_squared_frac / squared_frac[j] > 3 || squared_frac[j] / highest_squared_frac > snr_thresh)
			{
				a_16[j] = 0;
				entries -= 1;
				flag_entry_dismissed = true;
			}

	average_smooth = 0;
	// assess average topo sum for loudness
	for (int j = 0; j < 16; j++)
		average_smooth += a_16[j];
	average_smooth = int((float(average_smooth) / float(entries)) + 0.5);
}

// ---------------------------- DEBUG FUNCTIONS ------------------------------
void Globals::print_to_console(String message_to_print) // print String
{
	if (Globals::do_print_to_console)
		Serial.print(message_to_print);
}

void Globals::print_to_console(int int_to_print) // print int
{
	if (Globals::do_print_to_console)
		Serial.print(int_to_print);
	else if (Globals::do_send_to_processing)
		Serial.write(int_to_print);
}

void Globals::print_to_console(float float_to_print) // print float
{
	if (Globals::do_print_to_console)
		Serial.print(float_to_print);
}

void Globals::println_to_console(String message_to_print)
{
	if (Globals::do_print_to_console)
		Serial.println(message_to_print);
}

void Globals::println_to_console(int int_to_print) // print int
{
	if (Globals::do_print_to_console)
		Serial.println(int_to_print);
}

void Globals::println_to_console(float float_to_print) // print float
{
	if (Globals::do_print_to_console)
		Serial.println(float_to_print);
}

void Globals::printTopoArray(TOPOGRAPHY *topography)
{
	// print layer:
	print_to_console(topography->tag);
	print_to_console(":\t[");

	for (int j = 0; j < 16; j++)
	{
		print_to_console(topography->a_16[j]);
		if (j < 15)
			print_to_console(",");
	}

	print_to_console("] \tø");
	print_to_console(topography->average_smooth);
	print_to_console(" (");
	print_to_console(topography->average_smooth);
	print_to_console("/");
	print_to_console(topography->activation_thresh);
	print_to_console(")");

	if (topography->flag_entry_dismissed)
		print_to_console("\tentries dismissed!");
	if (topography->tag == "r")
	{
		print_to_console("regularity = ");
		print_to_console(topography->regularity);
	}
	if (topography->flag_empty_increased)
		print_to_console("\tempty slot repeatedly not played; regularity++");
	if (topography->flag_occupied_increased)
		print_to_console("\toccupied slot repeatedly played; regularity++");
	if (topography->flag_occupied_missed)
		print_to_console("\toccupied slot missed!; regularity--");
	if (topography->flag_empty_played)
		print_to_console("\tempty slot PLAYED!; regularity--");

	println_to_console("");
}

// ----------------------------- Auxiliary --------------------------

String Globals::DrumtypeToHumanreadable(DrumType type)
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
		return "S_Tom1";
	case Standtom2:
		return "S_Tom2";
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

String Globals::EffectstypeToHumanReadable(EffectsType type)
{
	switch (type)
	{
	case PlayMidi:
		return "PlayMidi";
	case Monitor:
		return "Monitor";
	case ToggleRhythmSlot:
		return "ToggleRhythmSlot";
	case FootSwitchLooper:
		return "FootSwitchLooper";
	case TapTempo:
		return "TapTempo";
	case Swell:
		return "Swell";
	case TsunamiLink:
		return "TsunamiLink";
	case CymbalSwell:
		return "CymbalSwell";
	case TopographyMidiEffect:
		return "TopographyMidiEffect";
	case Change_CC:
		return "Change_CC";
	case Random_CC_Effect:
		return "Random_CC_Effect";
	case MainNoteIteration:
		return "MainNoteIteration";
	}
	return "";
}

CC_Type Globals::int_to_cc_type(int integer)
{

	// TODO: This has to be set for each instrument accordingly.
	
	switch (integer)
	{
	case 18:
		return Osc2_semitone;
	case 19:
		return Osc2_tune;
	case 20:
		return Mix_Level_1;
	case 21:
		return Mix_Level_2;
	case 28:
		return Patch_1_Depth;
	case 30:
		return Patch_3_Depth;
	case 44:
		return Cutoff;
	case 46:
		return LFO_Rate;
	case 71:
		return Resonance;
	case 50:
		return Amplevel;
	case 23:
		return Attack;
	case 25:
		return Sustain;
	case 26:
		return Release;
	case 51:
		return DelayTime;
	case 94:
		return DelayDepth;

	// ATTENTION:
	// IMPORTANT:
	// TODO: define all CC channels, otherwise this procedure can take forever in the while loop!
	default:
		Globals::print_to_console("could not convert integer ");
		Globals::print_to_console(integer);
		Globals::println_to_console("to CC_Type. Reset to None.");
		return None;
	}
}