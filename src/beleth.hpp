#pragma once

#include "rack.hpp"

namespace beleth {
    static const float kRadius = 35.f;

    static const float kScaleFactor = 15.f;

    static const float kPhiFactor = 2.f * M_PI / 12.f;

    static const int kMaxParts = 7;
    static const int kTonnetzRows = 12;
    static const int kMaxNotes = 12;

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

    static const std::string kNoteOutputLabel = "Chord part %d";

    static const int CircleOfFifths[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

    enum ChordTypes {
        CHORD_MAJOR,
        CHORD_MINOR,
        CHORD_AUGMENTED,
        CHORD_DIMINISHED,
        CHORD_SUSPENDED
    };

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
}