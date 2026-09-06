# DreamerDemo

A VCV Rack module that performs demonstration videos from a script.

You write what happens as a markdown file — patch this to that, turn this knob, open that menu — and the module performs it: a drawn pointer travels to each control, a narrated line is spoken in a rendered voice, and the whole take is recorded to an MP4 that needs no editing.

It exists because making a demonstration video by hand means watching a pointer you are moving, judging what is on screen, and trimming a timeline afterwards. A script has none of those problems. It is written as text, it performs identically every time, and the recording starts and stops with the run, so the file is already exactly the take.

**macOS on Apple Silicon.** See *Porting* below: the machinery is portable, and three specific things are not.

---

## Building

Needs the [Rack SDK](https://vcvrack.com/downloads) unpacked beside this folder as `../Rack-SDK`.

```
make            # build plugin.dylib
make dev        # build and install into Rack's user plugins folder
```

`make dev` writes straight into `~/Library/Application Support/Rack2/plugins-mac-arm64/DreamerDemo` rather than building a package. Quit Rack, run it, start Rack — a package copied while Rack is running can be cleared at shutdown before it is ever unpacked, so the next start silently loads the old build.

## Using it

Place the **Demo** module. Its panel carries everything for setting a demo up; a small runner window carries the three controls needed while one is playing.

- **Script** — the list of scripts. Every `.md` in `~/Library/Application Support/Rack2/DreamerDemo/scripts` is offered, most recently edited first. The plugin installs its examples there the first time it looks.
- **Reload** — read the script again after editing it, and render any line whose wording has changed.
- **Patch** — open the script's own patch as though you had opened it from the File menu, so it can be corrected and saved back with Command-S.
- **Run** — open the script's patch, wait a second, and perform it. **Escape** stops a take; pressing it again closes the runner and hands your own patch back.
- **Record** — arm the recorder. The next run writes an MP4 to your Downloads folder and closes it when the take ends.
- **Rate**, **Voice**, **Badges**, **Captions** — the speed multiplier, whether lines are spoken, whether each gesture is named beside the pointer, and whether the narration is also shown as a card.

Your own patch is saved when a run starts and restored when the demo is finished with, so running a demo never costs you what you were working on.

### Recording

Recording uses ScreenCaptureKit and needs **Screen Recording** permission, granted to Rack itself in System Settings under Privacy & Security. macOS asks the first time and Rack must be restarted afterwards; it is granted once and stays granted.

The file contains Rack's window and Rack's own audio — the patch and the narration together, already in sync — at up to 2048 pixels wide and thirty frames a second.

### Voices

Lines are rendered by macOS's `say` and cached, so a line is spoken to disk once and played from there. A script names its voice; if that voice is not installed the system's own is used instead and a line is written to the log. Premium voices are downloaded in System Settings under Accessibility, Spoken Content.

---

## Writing a script

A script is a markdown file. Each `##` heading is a step carrying a move; the prose beneath it is what is said there. A step with no prose says nothing but still takes its pause.

```markdown
# Chart — reading a lead sheet

**Patch** ../patches/chart.vcv
**Modules** chart = DreamerMPX/mpxChart, quant = Fundamental/Quantizer
**Voice** Karen (Premium)
**Rate** 193
**Master** audio:Level
**Duck** 10 dB
**Pacing** perform 0.7, arrive 0.4, beat 0.3, settle 0.4, hold 0.4, tail 1.0

## zoom chart 1.4
This is the chart module. Its chord output carries the tones of the bar.

## patch chart:"Chord tones" -> quant:in
## set chart:Tempo 120
## wait 4
```

### Steps

```
say                            nothing happens; the prose is the step
wait <seconds>                 let the patch play
point <target>                 go there and say the note; touch nothing
press <target>                 left click it
right <target>                 right click it, which opens its menu
set <target> <value>           a fraction, a percentage, or a word
scroll <target> [up|down] [n]  n notches of a wheel, one by default
patch <target> -> <target>     either way round
drag <target> -> <target>      press on one, let go on the other
unpatch <target>               pull the cable off and drop it
menu <target> <item> [<item>]  right click, then click each row in turn
move <name> <hp> <rows>        drag a module by its panel
zoom <name> [factor|start]     frame a module; `zoom out` frames the whole rack
pan <name>                     centre on it without changing how close it is
open <patch file>              load a patch
add <Plugin/Model> as <name>   add a module and bind a name to it
key <name>                     press a key where the pointer is standing
```

### Addressing controls

A target is `name` for a module, or `name:control` for one of its controls, where the control is the name the module gave that parameter or port with `configParam`, `configInput` or `configOutput` — the same name Rack shows in a tooltip. Nothing in a script depends on where anything is on screen, so one script survives any window size, zoom, or rearranged rack.

```
chart:Tempo                the knob called Tempo
chart:"Chord tones"        quoted, because the name has a space in it
tona:in:FM                 force the input side, where a knob shares the name
vco:out:#1                 by number, for a module that names nothing
chart@50%,12%              a place on its panel, as fractions of its size
widget@30,9                pixels from the left and top; negative from the right
widget:vco:Sine            the clip-on widget on that port (Test Gear only)
window:close               the close corner of a floating window a module opened
```

### Pacing

`perform` is how long the pointer takes to travel, `arrive` the pause after it lands, `beat` how long the gesture is named before it fires, `settle` the pause afterwards, `hold` how long a new note stays up, and `tail` the silence after a spoken line. A note holds for the longer of `hold` and however long its line takes to say, and `tail` is added after whichever wins.

---

## Porting

Everything that decides what happens — the step machine, the resolver, the camera, the card, the drawn pointer, the script parser — is ordinary C++ against Rack's own API and would compile anywhere.

Three things are macOS:

| File | What it does | Needed elsewhere |
| --- | --- | --- |
| `src/Speech.cpp` | renders lines with `say`, measures them with `afinfo` | any text-to-speech that writes a file |
| `src/Capture.mm` | records the window with ScreenCaptureKit | any screen recorder that writes an MP4 |
| `src/Capture.mm` | plays a rendered line through AVAudioEngine | any in-process audio playback |

`src/capture_other.cpp` already carries the stubs for the second and third, and each interface is four functions wide — see `Capture.hpp` and `Speech.hpp`. A Windows or Linux port is implementing those, not porting the plugin.

The narration must be played **in Rack's own process**. It was played by a separate program at first, and the system's audio capture does not pick a new process up the moment it starts making a noise, so every recording lost its opening sentence.

---

## What it is not

Not a library plugin. It reads scripts from a folder on disk, injects events into the host's interface and starts a process to speak, none of which belongs in something thousands of people install. It is built from source by whoever wants it.

Not a test framework, though the same machinery would serve as one — every step checks its own result and a run stops on the step that failed rather than carrying on against a patch that was never made.

`docs/design.md` is the design document: what each decision is, and what was tried before it.
