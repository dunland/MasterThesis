#include <Instruments.h>
#include <MIDI.h>

void Instrument::trigger(Instrument* instrument, midi::MidiInterface<HardwareSerial> MIDI)
{
    switch (effect)
    {
    case PlayMidi:
        Effect::playMidi(instrument, MIDI);
        break;

    case Monitor:
        Effect::monitor(instrument);
        break;

    case ToggleRhythmSlot:

        break;

    case FootSwitchLooper:

        break;

    case TapTempo:

        break;

    case Swell:

        break;

    case TsunamiLink:

        break;

    case CymbalSwell:

        break;
    case TopographyLog:

        break;

    default:
        break;
    }
}