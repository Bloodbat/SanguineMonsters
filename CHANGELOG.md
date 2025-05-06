# 2.4.0

## New Modules

### Chronos

- Polyphonic collection of four low-frequency oscillators.

### Crucible

- Expander for Alchemist: adds global and per channel CV and manual  Mute control; Mute exclusive mode; global and per channel CV and manual Solo control, and Solo exclusive mode.

### Denki

- Polyphonic expander for Kitsune: adds Gain and Offset CV control.

### Fortuna

- Dual polyphonic signal destination randomizer: like Bernoulli gates; but for signals.

### Manus

- Expander for Gegeenes and Hydra that provides CV trigger step selection.

---

## Fixes

- Plugin: faceplate colors.

- Plugin: output gate glypg drawing.

- Dungeon: module was falling off the Rack: it was missing screws.

- Dungeon: actually store user preference when "Store voltage in patch" is disabled.

- Gegeenes: button lights turning on when steps are clicked; single shot has ended, and no reset has been received.

- Gegeenes: store and restore "One Shot" user preference.

- Hydra: button lights turning on when steps are clicked; single shot has ended, and no reset has been received.

- Hydra: store and restore "One Shot" user preference.

---

## Additions

- Aion: faceplate knob borders and knob labels.

- Alchemist: knob border.

- Alchemist: ability to add expander from context menu.

- Alchemist: master mute button and CV input.

- Brainz: knob borders.

- Bukavac: knob borders.

- Chronos: knob borders.

- Dolly-X: knob borders.

- Dungeon: optional moon halo (on by default).

- Dungeon: knob border.

- Gegeenes: knob border.

- Gegeenes: Manus expander.

- Hydra: knob border.

- Hydra: Manus expander.

- Kitsune: smart input normalling (can be turned off using the context menu or the switch on the faceplate, should not impact patches using old 1->2, 3->4 normalling).

- Raiju: knob borders.

- Sphinx: knob borders.

- Werewolf: mix left and right output audio if both inputs are connected and only one output is connected.

- Werewolf: knob borders.

- Werewolf: normal left and right inputs to each other.

- Werewolf: normal left and right outputs to each other.

---

## Changes

- Plugin: performance tweaks.

- Plugin: bring indicator lights in line with the ones from the Mutants.

- Plugin source: move common Sanguine Modules sources to submodule.

- Plugin: faceplate color tweaks.

- Aion: faceplate tweaks.

- Alchemist: use light bezels instead of light buttons.

- Alchemist: tweak faceplate.

- Alembic: faceplate tweaks.

- Brainz: faceplate tweaks.

- Brainz: performance tweaks.

- Brainz: better tooltips.

- Bukavac: faceplate tweaks.

- Bukavac: attenuators into attenuverters with more traditional response.

- Chronos: faceplate tweaks.

- Dolly-X: faceplate tweaks.

- Dungeon: better moon light colors.

- Gegeenes: performance tweaks.

- Hydra: performance tweaks.

- Kitsune: prettify faceplate.

- Kitsune: expand indicator light range to 10V.

- Oraculus: faceplate tweaks.

- Raiju: make it clear every output can be polyphonic.

- Sphinx: reorganize faceplate.

- Werewolf: reorganize faceplate.


---

# 2.3.2

## Fixes

- Manual link.

## Changes

- Plugin: separate Sanguine Monsters and Sanguine Mutants in the browser. Monsters are grouped in the "Sanguine Monsters" brand.


---

# 2.3.1

## Additions

- Theme support for existing modules with two available options: "Vitriol" the usual, colorful faceplate or "Plumbago" a black as night variation.

- Polyphonic ports are now shown with golden jacks.

## Fixes

- Plugin no longer crashes Rack Pro when it is used as a guest of hosts such as Reaper and Ableton and patches are loaded.

- Plugin no longer crashes Rack when used headless.

- Display rendering order.

- Werewolf Gain and Fold now respect polyphonic CV.


---

# 2.3.0

## New Modules

### Dungeon

A sample and hold; track and hold; hold and track, and white noise source.

### Kitsune

A quad attenuverter, offsetter and inverter.

### Oubliette

A null sink and cable parking for your wires. Also works as a null voltage source.

### Medusa

A normalled polyphonic mega multiple with 32 inputs and outputs.

### Aion

A quad timer/counter that can be triggered externally or has an internal seconds clock.

### Werewolf

A polyphonic, stereo, dirty distortion.

### Alchemist

A mixer for up to 16 channels gathered from a polyphonic cable. Mixed monophonic and polyphonic outputs are provided.

## Additions

- Sphinx can now run in reverse.

- Leave displays on in module browser.

## Changes

- Use float_4 for Dolly-X voltages.

- Use float_4 for Gegenees voltages.

- Use float_4 for Hydra voltages.

- Better changelog layout.

- Drop prefix from module names in browser.

- Shaped lights don't turn off in module browser.

- Use less disk space for shaped lights and buttons.

## Fixes

- Switches output 0V when their input is disconnected.

- Sphinx won't output an EOC trigger on the first round after starting or reset.

- Brainz CV trigger detection.


---

# 2.2.0

## New Modules

### Bukavac

A monster with a plethora of noise colors... and Perlin.

## Changes

- Brainz module direction button.

## Fixes

- Sphinx display not showing up in module browser or when module is bypassed.

- Light behavior.


---

# 2.1.1

## Fixes

- Sphinx fix


---

# 2.1.0

## New Modules

### Brainz

A utility to control multiple recorders and ease synchronizing their outputs in post.

### Sphinx

A multimode sequencer.

### Raiju

A fixed voltages source.

### Oraculus

A polyphonic to monophonic switch.

### Dolly-X

A monophonic to polyphonic channel cloner.

## Additions

- Disabled steps are now grayed out in Hydra and Gegenees.

## Changes

- Move manual to manuals repository.

- Mutants and Monsters can now be found under Sanguine Modules in the browser.

## Fixes

- Animation should not play when clicking step buttons in Hydra and Gegenees.


---

# 2.0.0

First release.