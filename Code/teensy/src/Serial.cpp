#include <Serial.h>
#include <Arduino.h>
#include <vector>
#include <MIDI.h>
#include <Tsunami.h>
#include <ArduinoJson.h>
#include <Globals.h>
#include <Instruments.h>
#include <Hardware.h>
#include <Score.h>

void JSON::compose_and_send_json(std::vector<Instrument *> instruments)
{
    size_t capacity = JSON_ARRAY_SIZE(Score::notes.size()) + 9 * JSON_ARRAY_SIZE(16) + 9 * JSON_OBJECT_SIZE(8) + 2 * JSON_OBJECT_SIZE(9);
    DynamicJsonDocument doc(capacity);

    // ---------------------- Global values ---------------------------
    JsonObject score = doc.createNestedObject("score");
    score["millis"] = millis();
    score["current_beat_pos"] = Globals::current_beat_pos;
    score["score_step"] = Score::step;

    // -------------------------- Score -------------------------------
    JsonArray score_notes = score.createNestedArray("notes");
    for (uint8_t i = 0; i < Score::notes.size(); i++)
    {
        score_notes.add(Score::notes[i]);
    }
    score["note"] = Score::notes[Score::note_idx];

    // Score topographies:
    JsonArray score_topo = score.createNestedArray("topo");
    for (uint8_t i = 0; i < Score::beat_sum.a_16.size(); i++)
    {
        score_topo.add(Score::beat_sum.a_16[i]);
    }

    // Score topography values:
    score["topo_ready"] = Score::beat_sum.ready();                  // TODO: replace with beat_regularity at some point
    score["average_smooth"] = Score::beat_sum.average_smooth;       // TODO: replace with beat_regularity at some point
    score["activation_thresh"] = Score::beat_sum.activation_thresh; // TODO: replace with beat_regularity at some point

    // --------------------------- Instruments ------------------------
    for (auto &instrument : instruments)
    {
        JsonObject instr = doc.createNestedObject(Globals::DrumtypeToHumanreadable(instrument->drumtype));

        JsonArray topo = instr.createNestedArray("topo");

        // instrument topography:
        for (uint8_t i = 0; i < instrument->topography.a_16.size(); i++)
        {
            topo.add(instrument->topography.a_16[i]);
        }

        // topo information:
        instr["average_smooth"] = instrument->topography.average_smooth;
        instr["activation_thresh"] = instrument->topography.activation_thresh;

        // instrument values:
        instr["wasHit"] = instrument->wasHit;
        instr["cc_val"] = instrument->midi_settings.cc_val;
        instr["cc_increase"] = instrument->midi_settings.cc_increase_factor;
        instr["cc_decay"] = instrument->midi_settings.cc_decay_factor;
        instr["effect"] = Globals::EffectstypeToHumanReadable(instrument->effect);
    }

    // transmit information:
    serializeJson(doc, Serial);
    Serial.println("");
}