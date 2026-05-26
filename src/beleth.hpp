#pragma once

#include "rack.hpp"

namespace beleth {
    static const float kRadius = 35.f;

    static const float kScaleFactor = 17.f;

    static const float kPhiFactor = 2.f * M_PI / 12.f;

    static const int kMaxParts = 7;
    static const int kTonnetzRows = 12;
    static const int kMaxNotes = 12;

    static const int kLightsFrequency = 16;

    static const std::vector<std::string> chordGroupLabels = {
        "Augmented / diminished",
        "Major / minor"
    };

    static const std::vector<std::string> partsLabels = {
        "3",
        "4",
        "5",
        "6",
        "7"
    };

    static const std::vector<std::string> transposeLabels = {
        "0 semitones",
        "1 semitones",
        "2 semitones",
        "3 semitones",
        "4 semitones",
        "5 semitones",
        "6 semitones",
        "7 semitones",
        "8 semitones",
        "9 semitones",
        "10 semitones",
        "11 semitones"
    };

    static const std::vector<std::string> voicingLabels = {
        "-4",
        "-3",
        "-2",
        "-1",
        "0",
        "1",
        "2",
        "3",
        "4"
    };

    static const char* noteNames[12] = {
         "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    static const std::string kNoteOutputLabel = "Chord part %d";

    static const int CircleOfFifths[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

    enum ChordTypes {
        CHORD_MAJOR,
        CHORD_MINOR,
        CHORD_AUGMENTED,
        CHORD_DIMINISHED,
        CHORD_SUSPENDED
    };

    // Red background.
    static const NVGcolor displayColorBackground = nvgRGB(48, 16, 16);
    static const NVGcolor displayColorActive = nvgRGB(255, 0, 0);
    static const NVGcolor displayColorTriangle = nvgRGB(127, 16, 16);
    static const NVGcolor displayColorChordPath = nvgRGB(255, 16, 16);
    static const NVGcolor displayColorGrid = nvgRGB(255, 32, 32);

    struct Note {
        // Projected position.
        float x;
        float y;

        float active;
        int pitchClass;

        // Logical position.
        int noteX;
        int noteY;
    };

    template <class T, size_t ROW, size_t COL>
    using NotesArray = T[ROW][COL];

    static void makeInitialNotes(beleth::NotesArray<beleth::Note, beleth::kTonnetzRows, beleth::kMaxNotes>& newNotes) {
        for (int row = 0; row < beleth::kTonnetzRows; ++row) {
            for (int note = 0; note < beleth::kMaxNotes; ++note) {
                newNotes[row][note].pitchClass = beleth::CircleOfFifths[(note + 10 + row * 8) % 12];

                float phi = -(static_cast<float>(note) - static_cast<float>(row) / 2.f) * beleth::kPhiFactor;
                float radius = beleth::kRadius + beleth::kScaleFactor * row;

                float x = radius * sin(phi);
                float y = radius * cos(phi);

                newNotes[row][note].x = x;
                newNotes[row][note].y = y;
                newNotes[row][note].noteX = note;
                newNotes[row][note].noteY = row;
            }
        }
    };

}