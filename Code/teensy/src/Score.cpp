#include <Score.h>
#include <MIDI.h>
#include <Instruments.h>
#include <Hardware.h>

TOPOGRAPHY Score::beat_regularity; // TODO: fully implement this.
TOPOGRAPHY Score::beat_sum;
TOPOGRAPHY Score::topo_midi_effect;

int Score::step = 0;
int Score::note_idx = 0;        // points at active (bass-)note
int Score::note_change_pos = 0; // defines at what position to increase note_idx
std::vector<int> Score::notes;

boolean Score::setup = true; // when true, current score_step's setup function is executed.

void Score::add_bassNote(int note)
{
    notes.push_back(note);
    Globals::print_to_console("note ");
    Globals::print_to_console(note);
    Globals::print_to_console(" has been added to Score::notes [ ");
    for (uint8_t i = 0; i < notes.size(); i++)
    {
        Globals::print_to_console(notes[i]);
        Globals::print_to_console(" ");
    }
    Globals::println_to_console("]");
}

void Score::proceed_to_next_step(std::vector<Instrument *> instruments)
{
    // increase step:
    Score::step++; // go to next score step
    Score::setup = true;

    // reset instrument topographies:
    for (auto &instrument : instruments)
        for (int j = 0; j < 16; j++)
            instrument->topography.a_16[j] = 0;

    // reset overall beat sum:
    for (int j = 0; j < 16; j++)
        Score::beat_sum.a_16[j] = 0; // reset topography
    Score::beat_sum.average_smooth = 0;

    Globals::println_to_console("regularity height > 10: reset!");
    Globals::println_to_console("all instrument topographies were reset.");
}

void Score::set_notes(std::vector<int> list)
{
    // clear notes list:
    notes.clear();

    // add notes from list:
    for (uint8_t i = 0; i < list.size(); i++)
    {
        notes.push_back(list[i]);
    }

    // print list:
    Globals::print_to_console("Score::notes:");
    for (uint8_t i = 0; i < notes.size(); i++)
    {
        Globals::print_to_console(" ");
        Globals::print_to_console(notes[i]);
    }
    Globals::println_to_console("");
}

///////////////////////////////////////////////////////////////////////
////////////////////////////// MODES //////////////////////////////////
///////////////////////////////////////////////////////////////////////

// play note, repeatedly:
void Score::playRhythmicNotes(Synthesizer *synth, midi::MidiInterface<HardwareSerial> MIDI, int rhythmic_iterator) // initiates a continuous bass note from score
{
    if (rhythmic_iterator != 0)
        note_change_pos = rhythmic_iterator;

    if ((Globals::current_beat_pos + 1) % note_change_pos == 0)
    {
        // play note
        synth->sendNoteOff(notes[note_idx], MIDI);
        synth->sendNoteOn(notes[note_idx], MIDI);
        Globals::print_to_console("\tplaying Score::note:");
        Globals::println_to_console(notes[note_idx]);

        // change note
        if (notes.size() > 1)
        {
            note_idx++;
            if (note_idx > int(notes.size()) - 1)
                note_idx = 0;
            //  = (note_idx + 1) % notes.size(); // iterate through the bass notes
            Globals::print_to_console("\tnote_idx = ");
            Globals::println_to_console(note_idx);
        }
    }
    // else

    // if (Globals::current_beat_pos == 0) // at beginninng of each bar
    // {
    //     Globals::print_to_console("\tnotes.size() = ");
    //     Globals::println_to_console(int(notes.size()));
    // }
}

// play note only once (turn on never off):
void Score::playSingleNote(Synthesizer *synth, midi::MidiInterface<HardwareSerial> MIDI) // initiates a continuous bass note from score
{
    if (notes.size() > 0)
    {
        // turn previously played note off:
        int prev_idx_temp = (note_idx > 0) ? note_idx - 1 : notes.size() - 1;
        synth->sendNoteOff(notes[prev_idx_temp], MIDI);

        // play nex note:
        synth->sendNoteOn(notes[note_idx], MIDI);
    }
    else
        Globals::println_to_console("cannot play MIDI note, because Score::notes is empty.");
}

// play note at a set position in bar:
void Score::playNoteAtPosition(Synthesizer *synth, midi::MidiInterface<HardwareSerial> MIDI, int note_change_pos)
{
    if (Globals::current_beat_pos == note_change_pos)
    {
        // turn previously played note off:
        int prev_idx_temp = (note_idx > 0) ? note_idx - 1 : notes.size() - 1;
        synth->sendNoteOff(notes[prev_idx_temp], MIDI);

        // play nex note:
        synth->sendNoteOn(notes[note_idx], MIDI);
    }
}

void Score::envelope_cutoff(Synthesizer *synth, TOPOGRAPHY *topography, midi::MidiInterface<HardwareSerial> MIDI)
{
    int cutoff_val = topography->a_16[Globals::current_16th_count] * 13; // create cutoff value as a factor of topography height

    cutoff_val = max(20, cutoff_val);  // must be at least 20
    cutoff_val = min(cutoff_val, 127); // must not be greater than 127
    synth->sendControlChange(Cutoff, cutoff_val, MIDI);
}

void Score::envelope_volume(TOPOGRAPHY *topography, midi::MidiInterface<HardwareSerial> MIDI, Synthesizer *synth)
{
    int amp_val = topography->a_16[Globals::current_16th_count] * 13; // create cutoff value as a factor of topography height
    // amp_val = max(0, amp_val);                                    // must be at least 0
    amp_val = min(amp_val, 127); // must not be greater than 127
    synth->sendControlChange(Amplevel, amp_val, MIDI);
}

void Score::crazyDelays(Instrument *instrument, midi::MidiInterface<HardwareSerial> MIDI, Synthesizer *synth)
{
    int delaytime = instrument->topography.a_16[Globals::current_16th_count] * 13; // create cutoff value as a factor of topography height
    delaytime = min(delaytime, 127);                                               // must not be greater than 127
    synth->sendControlChange(DelayTime, delaytime, MIDI);
}

// void Score::set_ramp(midi::MidiInterface<HardwareSerial> MIDI, CC_Type cc_type, MIDI_Instrument midi_instr, int start_value, int end_value, int duration)
// {
//     static boolean ramp_start = true;
//     static int value;
//     static unsigned long start_time;
//     static float factor;

//     if (ramp_start)
//     {
//         value = start_value;
//         start_time = millis();
//         factor = (start_value - end_value) / duration;
//         ramp_start = false;
//     }

//     MIDI.sendControlChange(cc_type, value, midi_instr);
//     value += factor;

//     if (millis() > start_time + duration)
//     {

//     }
// }
