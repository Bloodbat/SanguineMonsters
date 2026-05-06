#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

#include <array>

#include "monsterscommon.hpp"
#include "beleth.hpp"

using namespace sanguineCommonCode;

struct Beleth : SanguineModule {
    enum ParamIds {
        PARAM_CHORD_GROUP,
        PARAM_SUSPENDED,
        PARAM_TRANSPOSE,
        PARAM_PARTS,
        PARAM_VOICING,
        PARAMS_COUNT
    };
    enum InputIds {
        INPUT_PERFECT_FIFTH,
        INPUT_MAJOR_TRIAD,
        INPUT_TRANSPOSE,
        INPUT_VOICING,
        INPUT_PARTS,
        INPUT_CHORD_GROUP,
        INPUT_SUSPENDED,
        INPUTS_COUNT
    };
    enum OutputIds {
        OUTPUT_TONIC,
        ENUMS(OUTPUT_PART, beleth::kMaxParts),
        OUTPUTS_COUNT
    };

    enum LightIds {
        ENUMS(LIGHT_CHORD_GROUP, 2),
        LIGHT_SUSPENDED,
        LIGHTS_COUNT
    };

    int octaves[beleth::kMaxParts];
    int parts;
    int voicing;
    int transpose;

    static const int kLightsFrequency = 16;
    int jitteredLightsFrequency;

    beleth::ChordTypes chordType;

    // Logical position of the playhead.
    int noteX;
    int noteY;

    bool bHaveParts = false;
    bool bHaveChordGroup = false;
    bool bWantMajorMinor;
    bool bHaveSuspended = false;
    bool bWantSuspended;

    // Geometrical position of the playhead.
    float x0;
    float y0;

    /*
    Tonnetz torus aligned to major thirds
    -> three rows are enough, fourth is for display, twelve completes the map
    */
    beleth::NotesArray<beleth::Note, beleth::kTonnetzRows, beleth::kMaxNotes> notes;
    std::array<beleth::Note*, beleth::kMaxParts> chords;

    dsp::ClockDivider lightsDivider;

    Beleth() {
        config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

        init();

        configSwitch(PARAM_CHORD_GROUP, 0.f, 1.f, 0.f, "Chord groups", beleth::chordGroupLabels);
        configSwitch(PARAM_SUSPENDED, 0.f, 1.f, 0.f, "Suspended chords (sus4)",
            monsterscommon::onOffButtonLabels);
        configParam(PARAM_TRANSPOSE, 0.f, 1.f, 0.f, "Transpose");
        configSwitch(PARAM_PARTS, 3.f, 7.f, 7.f, "Parts", beleth::partsLabels);
        configParam(PARAM_VOICING, -1.f, 1.f, 0.f, "Voicing");

        configInput(INPUT_PERFECT_FIFTH, "Perfect fifth per volt");
        configInput(INPUT_MAJOR_TRIAD, "Major triad per volt");
        configInput(INPUT_TRANSPOSE, "Transpose");
        configInput(INPUT_VOICING, "Voicing");
        configInput(INPUT_PARTS, "Parts");
        configInput(INPUT_CHORD_GROUP, "Chord groups");
        configInput(INPUT_SUSPENDED, "Suspended chords (sus4)");

        configOutput(OUTPUT_TONIC, "Tonic");

        configOutput(OUTPUT_PART, string::f(beleth::kNoteOutputLabel, 1));
        configOutput(OUTPUT_PART + 1, string::f(beleth::kNoteOutputLabel, 3));
        configOutput(OUTPUT_PART + 2, string::f(beleth::kNoteOutputLabel, 5));
        configOutput(OUTPUT_PART + 3, string::f(beleth::kNoteOutputLabel, 7));
        configOutput(OUTPUT_PART + 4, string::f(beleth::kNoteOutputLabel, 9));
        configOutput(OUTPUT_PART + 5, string::f(beleth::kNoteOutputLabel, 11));
        configOutput(OUTPUT_PART + 6, string::f(beleth::kNoteOutputLabel, 13));
    }

    void process(const ProcessArgs& args) override {
        if (!bHaveParts) {
            parts = params[PARAM_PARTS].getValue();
        } else {
            float partsVoltage = inputs[INPUT_PARTS].getVoltage();
            partsVoltage = round(partsVoltage);
            partsVoltage = clamp(partsVoltage, 0.f, 5.f);
            partsVoltage = rescale(partsVoltage, 0.f, 5.f, 3.f, 7.f);
            parts = static_cast<int>(partsVoltage);
        }

        if (!bHaveChordGroup) {
            bWantMajorMinor = static_cast<bool>(params[PARAM_CHORD_GROUP].getValue());
        } else {
            bWantMajorMinor = inputs[INPUT_CHORD_GROUP].getVoltage() >= 1.f;
        }

        if (!bHaveSuspended) {
            bWantSuspended = static_cast<bool>(params[PARAM_SUSPENDED].getValue());
        } else {
            bWantSuspended = inputs[INPUT_SUSPENDED].getVoltage() >= 1.f;
        }

        float voicingVoltage = inputs[INPUT_VOICING].getVoltage() / 10.f;
        float transposeVoltage = inputs[INPUT_TRANSPOSE].getVoltage() / 10.f;

        voicing = clamp(voicingVoltage +
            params[PARAM_VOICING].getValue(), -1.f, 1.f) * 4.f * parts;
        transpose = clamp(transposeVoltage +
            params[PARAM_TRANSPOSE].getValue(), 0.f, 1.f) * 11.f;

        x0 = inputs[INPUT_PERFECT_FIFTH].getVoltage() + 6;
        y0 = inputs[INPUT_MAJOR_TRIAD].getVoltage() + 1;

        while (x0 < 0.f) {
            x0 += 12.f;
        }
        while (x0 >= 12.f) {
            x0 -= 12.f;
        }

        while (y0 < 0.f) {
            y0 += 3.f;
        }
        while (y0 >= 3.f) {
            y0 -= 3.f;
        }

        // Determine active triad (diagonal division of logical square cell).
        noteX = floor(x0);
        noteY = floor(y0);

        if (bWantMajorMinor) {
            if (y0 - noteY > x0 - noteX) {
                chordType = beleth::CHORD_MAJOR;
            } else {
                chordType = beleth::CHORD_MINOR;
            }
        } else {
            if (y0 - noteY > x0 - noteX) {
                chordType = beleth::CHORD_AUGMENTED;
            } else {
                chordType = beleth::CHORD_DIMINISHED;
            }
        }

        // (m/M) (M/A) (A/M) (M/m) (m/d) (d/m).
        if (bWantSuspended && fabs(y0 - noteY) < 0.3f) {
            chordType = beleth::CHORD_SUSPENDED;
        }

        int noteYPlusOne;
        int modNoteYPlusOne;
        int modNoteX1;
        int modNoteX2;
        int modNoteX3;
        int augNoteY;
        switch (chordType) {
        case beleth::CHORD_MAJOR:
            // Major (0 4 7 e 2 6 9).
            noteYPlusOne = noteY + 1;
            modNoteX1 = (noteX + 1) % 12;
            modNoteX2 = (noteX + 2) % 12;
            modNoteYPlusOne = noteYPlusOne % beleth::kTonnetzRows;
            chords[0] = &notes[modNoteYPlusOne][noteX];
            chords[1] = &notes[noteY][noteX];
            chords[2] = &notes[modNoteYPlusOne][modNoteX1];
            chords[3] = &notes[noteY][modNoteX1];
            chords[4] = &notes[modNoteYPlusOne][modNoteX2];
            chords[5] = &notes[noteY][modNoteX2];
            chords[6] = &notes[modNoteYPlusOne][(noteX + 3) % 12];
            break;

        case beleth::CHORD_MINOR:
            // Minor (0 3 7 t 2 5 9).
            noteYPlusOne = noteY + 1;
            modNoteX1 = (noteX + 1) % 12;
            modNoteX2 = (noteX + 2) % 12;
            modNoteX3 = (noteX + 3) % 12;
            modNoteYPlusOne = noteYPlusOne % beleth::kTonnetzRows;
            chords[0] = &notes[noteY][noteX];
            chords[1] = &notes[modNoteYPlusOne][modNoteX1];
            chords[2] = &notes[noteY][modNoteX1];
            chords[3] = &notes[modNoteYPlusOne][modNoteX2];
            chords[4] = &notes[noteY][modNoteX2];
            chords[5] = &notes[modNoteYPlusOne][modNoteX3];
            chords[6] = &notes[noteY][modNoteX3];
            break;

        case beleth::CHORD_AUGMENTED:
            // Augmented (wrap??).
            augNoteY = 12 + noteY;
            for (int part = 0; part < beleth::kMaxParts; ++part) {
                chords[part] = &notes[(augNoteY - part) % 3][noteX];
            }
            break;

        case beleth::CHORD_DIMINISHED:
            // Diminished.
            for (int part = 0; part < beleth::kMaxParts; ++part) {
                chords[part] = &notes[(noteY + part) % 3][(noteX + part) % 12];
            }
            break;

        case beleth::CHORD_SUSPENDED:
            // Suspended (sus4).
            chords[0] = &notes[noteY][(noteX) % 12];
            chords[1] = &notes[noteY][(noteX + 2) % 12];
            chords[2] = &notes[noteY][(noteX + 1) % 12];
            chords[3] = &notes[noteY][(noteX + 4) % 12];
            chords[4] = &notes[noteY][(noteX + 3) % 12];
            chords[5] = &notes[noteY][(noteX + 6) % 12];
            chords[6] = &notes[noteY][(noteX + 5) % 12];
            break;
        }

        // Count octaves.
        int octave = 0;
        octaves[0] = octave;
        for (int part = 1; part < beleth::kMaxParts; ++part) {
            if (chords[part]->pitchClass < chords[part - 1]->pitchClass) {
                ++octave;
            }
            octaves[part] = octave;
        }

        // Add voicing.
        if (voicing > 0) {
            for (int i = 0; i < voicing; ++i) {
                ++octaves[i % parts];
            }
        } else {
            for (int i = 0; i < abs(voicing); ++i) {
                --octaves[i % parts];
            }
        }

        // Tonic.
        outputs[OUTPUT_TONIC].setVoltage(octaves[0] + transpose / 12.f);
        // Chord.
        for (int part = 0; part < parts; ++part) {
            outputs[OUTPUT_PART + part].setVoltage(static_cast<float>(octaves[part] +
                (transpose + chords[part]->pitchClass) % 12) / 12.f);
        }

        float rootNote = static_cast<float>(octaves[0] + (transpose + chords[0]->pitchClass) % 12) / 12.f;
        for (int part = parts; part < beleth::kMaxParts; ++part) {
            outputs[OUTPUT_PART + part].setVoltage(rootNote);
        }

        if (lightsDivider.process()) {
            const float sampleTime = args.sampleTime * jitteredLightsFrequency;

            lights[LIGHT_CHORD_GROUP].setBrightnessSmooth(!bWantMajorMinor *
                kSanguineButtonLightValue, sampleTime);

            lights[LIGHT_CHORD_GROUP + 1].setBrightnessSmooth(bWantMajorMinor *
                kSanguineButtonLightValue, sampleTime);

            lights[LIGHT_SUSPENDED].setBrightnessSmooth(bWantSuspended *
                kSanguineButtonLightValue, sampleTime);
        }
    }

    void init() {
        voicing = 0;
        params[PARAM_VOICING].setValue(0.f);
        parts = 7;
        params[PARAM_PARTS].setValue(7.f);
        params[PARAM_TRANSPOSE].setValue(0.f);
        transpose = 0;

        for (int row = 0; row < beleth::kTonnetzRows; ++row) {
            for (int note = 0; note < beleth::kMaxNotes; ++note) {
                notes[row][note].pitchClass = beleth::CircleOfFifths[(note + 10 + row * 8) % 12];

                float phi = -(static_cast<float>(note) - static_cast<float>(row) / 2.f) * beleth::kPhiFactor;
                float radius = beleth::kRadius + beleth::kScaleFactor * row;

                float x = radius * sin(phi);
                float y = radius * cos(phi);

                notes[row][note].x = x;
                notes[row][note].y = y;
                notes[row][note].noteX = note;
                notes[row][note].noteY = row;
            }
        }

        for (int part = 0; part < beleth::kMaxParts; ++part) {
            chords[part] = &notes[0][part];
            octaves[part] = 1;
        }
    }

    void onReset() override {
        init();
    }

    void onAdd(const AddEvent& e) override {
        jitteredLightsFrequency = kLightsFrequency + (getId() % kLightsFrequency);
        lightsDivider.setDivision(jitteredLightsFrequency);
    }

    void onPortChange(const PortChangeEvent& e) override {
        if (e.type == Port::INPUT) {
            switch (e.portId) {
            case INPUT_PARTS:
                bHaveParts = e.connecting;
                break;
            case INPUT_CHORD_GROUP:
                bHaveChordGroup = e.connecting;
                break;
            case INPUT_SUSPENDED:
                bHaveSuspended = e.connecting;
                break;

            default:
                break;
            }
        }
    }
};

struct BelethDisplay : TransparentWidget {
    Beleth* module = nullptr;

    std::shared_ptr<Font> font;

    float centerX;
    float centerY;

    float reducedBoxX;
    float reducedBoxY;

    explicit BelethDisplay(const math::Vec size) {
        box.size = size;
        reducedBoxX = box.size.x - 0.5f;
        reducedBoxY = box.size.y - 0.5f;

        centerX = reducedBoxX * 0.5f;
        centerY = reducedBoxY * 0.5f;
    }

    void draw(const DrawArgs& args) override {
        // Display background.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
        // TODO: display common?
        nvgFillColor(args.vg, nvgRGB(10, 10, 10));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.5f);
        nvgStrokeColor(args.vg, nvgRGB(100, 100, 100));
        nvgStroke(args.vg);

        Widget::draw(args);
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/DejaVuSansMono.ttf"));

            if (!font) {
                return;
            }

            if (module && !module->isBypassed()) {
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, 0.5f, 0.5f, reducedBoxX, reducedBoxY, 5.f);
                nvgFillColor(args.vg, beleth::displayColorBackground);
                nvgFill(args.vg);

                if (module->chordType == beleth::CHORD_MAJOR ||
                    module->chordType == beleth::CHORD_MINOR) {
                    drawChordTriads(args.vg, module->chords, module->parts);
                }

                drawTonnetzGrid(args.vg, module->notes);
                drawActiveChordPath(args.vg, module->chordType, module->chords, module->notes,
                    module->parts);
                drawActiveNotes(args.vg, module->chords);
                drawTonnetzNotes(args.vg, module->notes, module->transpose);

                // Draw playhead.
                float phi = -(module->x0 - module->y0 / 2.f) * beleth::kPhiFactor;
                float radius = beleth::kRadius + beleth::kScaleFactor * module->y0;
                float x = radius * sin(phi) + reducedBoxX * 0.5f;
                float y = radius * cos(phi) + reducedBoxY * 0.5f;
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, x, y, 2.04f);
                nvgFill(args.vg);
                nvgStrokeWidth(args.vg, 1.02f);
                nvgStroke(args.vg);

                drawRectHalo(args, box.size, beleth::displayColorActive, 55, 0.f);
            } else if (!module) {
                int fakeTranspose = 0;

                beleth::NotesArray<beleth::Note, beleth::kTonnetzRows, beleth::kMaxNotes> fakeNotes;

                for (int row = 0; row < beleth::kTonnetzRows; ++row) {
                    for (int note = 0; note < beleth::kMaxNotes; ++note) {
                        fakeNotes[row][note].pitchClass = beleth::CircleOfFifths[(note + 10 + row * 8) % 12];

                        float phi = -(static_cast<float>(note) - static_cast<float>(row) / 2.f) * beleth::kPhiFactor;
                        float radius = beleth::kRadius + beleth::kScaleFactor * row;

                        float x = radius * sin(phi);
                        float y = radius * cos(phi);

                        fakeNotes[row][note].x = x;
                        fakeNotes[row][note].y = y;
                        fakeNotes[row][note].noteX = note;
                        fakeNotes[row][note].noteY = row;
                    }
                }

                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, 0.5f, 0.5f, reducedBoxX, reducedBoxY, 5.f);
                nvgFillColor(args.vg, beleth::displayColorBackground);
                nvgFill(args.vg);

                drawTonnetzGrid(args.vg, fakeNotes);
                drawTonnetzNotes(args.vg, fakeNotes, fakeTranspose);
            }
        }
    }

    void drawChordTriads(NVGcontext* vg, const std::array<beleth::Note*, beleth::kMaxParts>& chords,
        const int& parts) {
        // Draw active chord - triangles.
        for (int part = 0; part < parts - 2; ++part) {
            nvgBeginPath(vg);
            nvgFillColor(vg, beleth::displayColorTriangle);
            nvgMoveTo(vg, chords[part]->x + centerX, chords[part]->y + centerY);
            nvgLineTo(vg, chords[part + 1]->x + centerX, chords[part + 1]->y + centerY);
            nvgLineTo(vg, chords[part + 2]->x + centerX, chords[part + 2]->y + centerY);
            nvgClosePath(vg);
            nvgFill(vg);
        }
    }

    void drawActiveChordPath(NVGcontext* vg, const beleth::ChordTypes& chordType,
        const std::array<beleth::Note*, beleth::kMaxParts>& chords,
        const beleth::NotesArray<beleth::Note, beleth::kTonnetzRows, beleth::kMaxNotes>& notes,
        const int& parts) {
        nvgStrokeWidth(vg, 1.36f);
        nvgStrokeColor(vg, beleth::displayColorChordPath);

        if (chordType == beleth::CHORD_MAJOR || chordType == beleth::CHORD_MINOR) {
            nvgBeginPath(vg);
            for (int part = 0; part < parts - 1; ++part) {
                nvgMoveTo(vg, chords[part]->x + centerX, chords[part]->y + centerY);
                nvgLineTo(vg, chords[part + 1]->x + centerX, chords[part + 1]->y + centerY);
            }
            nvgStroke(vg);
        } else if (chordType == beleth::CHORD_SUSPENDED) {
            nvgBeginPath(vg);
            float posX = chords[0]->x + centerX;
            float posY = chords[0]->y + centerY;
            nvgMoveTo(vg, posX, posY);
            int noteX = chords[0]->noteX;
            int noteY = chords[0]->noteY;
            for (int part = 0; part < parts; part++) {
                float x1 = notes[noteY][(noteX + part) % 12].x + centerX;
                float y1 = notes[noteY][(noteX + part) % 12].y + centerY;
                nvgLineTo(vg, x1, y1);
            }
            nvgStroke(vg);
        } else {
            for (int part = 0; part < parts; ++part) {
                nvgBeginPath(vg);
                float posX = chords[part]->x + centerX;
                float posY = chords[part]->y + centerY;
                nvgMoveTo(vg, posX, posY);
                int noteX = chords[part]->noteX;
                int noteY = chords[part]->noteY;
                if (chordType == beleth::CHORD_AUGMENTED) {
                    float x1 = notes[noteY + 1][noteX].x + centerX;
                    float y1 = notes[noteY + 1][noteX].y + centerY;
                    nvgLineTo(vg, x1, y1);
                }
                if (chordType == beleth::CHORD_DIMINISHED) {
                    float x1 = notes[noteY + 1][(noteX + 1) % 12].x + centerX;
                    float y1 = notes[noteY + 1][(noteX + 1) % 12].y + centerY;
                    nvgLineTo(vg, x1, y1);
                }
                nvgStroke(vg);
            }
        }
    }

    void drawTonnetzGrid(NVGcontext* vg, const beleth::NotesArray<beleth::Note,
        beleth::kTonnetzRows, beleth::kMaxNotes>& notes) {
        nvgStrokeWidth(vg, 0.51f);
        nvgStrokeColor(vg, beleth::displayColorGrid);

        for (int noteY = 0; noteY < 4; ++noteY) {
            for (int noteX = 0; noteX < 12; noteX++) {
                float x = notes[noteY][noteX].x + centerX;
                float y = notes[noteY][noteX].y + centerY;

                nvgBeginPath(vg);
                nvgMoveTo(vg, x, y);
                float x1 = notes[noteY][(noteX + 1) % 12].x + centerX;
                float y1 = notes[noteY][(noteX + 1) % 12].y + centerY;
                nvgLineTo(vg, x1, y1);
                if (noteY < 3) {
                    nvgMoveTo(vg, x, y);
                    x1 = notes[noteY + 1][(noteX) % 12].x + centerX;
                    y1 = notes[noteY + 1][(noteX) % 12].y + centerY;
                    nvgLineTo(vg, x1, y1);
                    nvgMoveTo(vg, x, y);
                    x1 = notes[noteY + 1][(noteX + 1) % 12].x + centerX;
                    y1 = notes[noteY + 1][(noteX + 1) % 12].y + centerY;
                    nvgLineTo(vg, x1, y1);
                }
                nvgStroke(vg);
            }
        }
    }

    void drawActiveNotes(NVGcontext* vg, const std::array<beleth::Note*, beleth::kMaxParts>& chords) {
        // Circles for played notes.
        nvgBeginPath(vg);
        nvgStrokeWidth(vg, 1.36f);
        nvgStrokeColor(vg, beleth::displayColorActive);
        nvgFillColor(vg, beleth::displayColorBackground);
        nvgCircle(vg, chords[0]->x + centerX, chords[0]->y + centerY, 5.44f);
        nvgFill(vg);
        nvgStroke(vg);
    }

    void drawTonnetzNotes(NVGcontext* vg, const beleth::NotesArray<beleth::Note,
        beleth::kTonnetzRows, beleth::kMaxNotes>& notes, const int& transpose) {
        // Names.
        for (int noteY = 0; noteY < 4; ++noteY) {
            for (int noteX = 0; noteX < 12; ++noteX) {
                float x = notes[noteY][noteX].x + centerX;
                float y = notes[noteY][noteX].y + centerY;

                // Circle with note name.
                nvgBeginPath(vg);
                nvgFillColor(vg, beleth::displayColorBackground);
                nvgCircle(vg, x, y, 4.08f);
                nvgFill(vg);

                nvgFontSize(vg, 7.5);
                nvgFontFaceId(vg, font->handle);
                Vec textPos = Vec(x - 2.08, y + 2);
                nvgFillColor(vg, beleth::displayColorActive);

                int pitchClass = (notes[noteY][noteX].pitchClass + transpose) % 12;

                nvgText(vg, textPos.x, textPos.y, beleth::noteNames[pitchClass], NULL);
            }
        }
    }
};

struct BelethWidget : SanguineModuleWidget {
    explicit BelethWidget(Beleth* module) {
        setModule(module);

        moduleName = "beleth";
        panelSize = sanguineThemes::SIZE_15;
        backplateColor = sanguineThemes::PLATE_PURPLE;
        bFaceplateSuffix = false;

        makePanel();

        addScrews(SCREW_ALL);

        FramebufferWidget* belethFrameBuffer = new FramebufferWidget();
        addChild(belethFrameBuffer);

        BelethDisplay* belethDisplay = new BelethDisplay(millimetersToPixelsVec(60.48f, 60.48f));
        belethDisplay->module = module;
        belethDisplay->box.pos = millimetersToPixelsVec(12.96f, 18.084f);
        belethFrameBuffer->addChild(belethDisplay);

        addParam(createLightParamCentered<VCVLightBezelLatch<GreenRedLight>>(
            millimetersToPixelsVec(7.584f, 27.316f), module, Beleth::PARAM_CHORD_GROUP,
            Beleth::LIGHT_CHORD_GROUP));
        addChild(createInputCentered<BananutBlack>(millimetersToPixelsVec(7.584f, 37.741f),
            module, Beleth::INPUT_CHORD_GROUP));

        addParam(createLightParamCentered<VCVLightBezelLatch<PurpleLight>>(
            millimetersToPixelsVec(7.584f, 57.026f), module, Beleth::PARAM_SUSPENDED,
            Beleth::LIGHT_SUSPENDED));
        addChild(createInputCentered<BananutBlack>(millimetersToPixelsVec(7.584f, 67.556f),
            module, Beleth::INPUT_SUSPENDED));

        addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.877f, 89.709f),
            module, Beleth::INPUT_TRANSPOSE));
        addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(29.018f, 89.709f),
            module, Beleth::PARAM_TRANSPOSE));

        addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(47.18f, 89.709f),
            module, Beleth::PARAM_VOICING));
        addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(69.315f, 89.709f),
            module, Beleth::INPUT_VOICING));

        addChild(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(21.492f, 104.468f),
            module, Beleth::PARAM_PARTS));
        addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(21.492f, 117.176f),
            module, Beleth::INPUT_PARTS));

        addChild(createInputCentered<BananutGreen>(millimetersToPixelsVec(6.877f, 104.468f),
            module, Beleth::INPUT_PERFECT_FIFTH));
        addChild(createInputCentered<BananutGreen>(millimetersToPixelsVec(6.877f, 117.014f),
            module, Beleth::INPUT_MAJOR_TRIAD));

        addChild(createOutputCentered<BananutRed>(millimetersToPixelsVec(36.113f, 104.468f),
            module, Beleth::OUTPUT_TONIC));

        static const float xSpacing = 11.067f;

        float xPos = 47.18f;
        for (int port = 0; port < 3; ++port) {
            addChild(createOutputCentered<BananutRed>(millimetersToPixelsVec(xPos, 104.468f),
                module, Beleth::OUTPUT_PART + port));
            xPos += xSpacing;
        }

        xPos = 36.113f;
        for (int port = 3; port < beleth::kMaxParts; ++port) {
            addChild(createOutputCentered<BananutRed>(millimetersToPixelsVec(xPos, 117.176f),
                module, Beleth::OUTPUT_PART + port));
            xPos += xSpacing;
        }
    }
};

Model* modelBeleth = createModel<Beleth, BelethWidget>("Sanguine-Monsters-Beleth");