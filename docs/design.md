# DreamerDemo — design

A VCV Rack plugin that plays an authored script of real actions — patching cables, turning knobs, adding modules, opening menus — while a synthetic pointer moves around the rack, a badge beside it names each gesture, and a card narrates what is happening in a rendered voice.

It exists to make demonstration videos. Recording a screen by hand means watching a pointer you are moving, judging what is visible, and editing a timeline afterwards, all of which are eyesight problems. A script has none of them: it is written as text, it performs identically every time, and a take needs no editing because the demo opens and closes on a title card.

Ported from DreamRack, whose `design/scripted-demo.md` describes the same subsystem inside a web app. The step model, the pacing rules, the badge vocabulary and the markdown script format come across as they are. What changes is the bottom layer — how a step reaches the application — and that recording and rendered speech both work differently here.

## What it is not

Not a test framework, though the same machinery would serve as one and the assertions below are the beginning of it.

Not something anyone else installs. It is never submitted to the VCV library. It reads scripts off a local disk, drives the host's interface and runs a command-line program to speak, none of which belongs in a plugin somebody downloads.

Not a recorder. ScreenFlow records the screen and the computer audio together; nothing here captures anything.

## Its own plugin

A plugin of its own rather than a module inside DreamerMPX.

The work it does is not music. It reaches into the host's widget tree, injects mouse events, and spawns a process to play audio — three things that would each be a fair question in a plugin under library review, and that would ship to every installer of MPX for the benefit of one person.

It is also not about MPX. The subject of a video is whatever is being shown that week, which will as often be someone else's modules or a patch technique as it will be this author's plugin. A demo plugin that lives inside the thing it demonstrates can only demonstrate that thing.

The cost is a second plugin to build and keep in step with the SDK. It shares no code with MPX.

## Announce, then do

The rule the whole thing is built around, and the reason a demo can be followed rather than merely watched.

Each step puts its note up, the pointer travels to the target and **waits there**, the badge names the gesture, and only then does the gesture happen. A fast movement you were told about is easier to follow than a slow one that surprises you. This ordering, rather than a slower overall rate, is what makes a demo readable.

## The step model

A script is a markdown file. One heading per step, carrying the move; ordinary prose beneath it, which is what is said there. A step with no prose says nothing but still takes its pause.

Markdown rather than JSON because the choreography of a demo settles in an afternoon and its wording is rewritten a dozen times, and rewriting a sentence inside JSON means minding quotes and escapes to change a word.

A header names the patch the script opens and binds short names to the modules it will address:

```markdown
# MPX Chart — reading a lead sheet

**Patch** demos/patches/chart-intro.vcv
**Modules** chart = DreamerMPX/mpxChart, quant = Fundamental/Quantizer
**Master** chart:P_TEMPO

## point chart:out.chord
The chord output carries the tones of the bar as a polyphonic pitch.

## patch chart:out.chord -> quant:in
## set chart:P_TEMPO 120
## wait 4
```

The step vocabulary, following DreamRack's:

    open <patch>                load a patch file
    add <model> as <name>       add a module and bind a name to it
    point <name>:<control>      move the pointer, say the note, do nothing
    press <name>:<control>      click a button or a switch
    set <name>:<control> <v>    move a knob or a slider to a value
    menu <name>:<control> <item>  right-click and choose from the menu
    patch a:b -> c:d            connect two ports
    unpatch c:d                 remove the cable at a port
    move <name> <x> <y>         drag a module by its panel
    zoom <name> [factor]        frame a module; `zoom out` returns to the whole rack
    say                         nothing happens; the note is the step
    wait <seconds>              let the patch play

Every step carries its own pacing, and the script's defaults supply whatever a step leaves out: how long the pointer takes to travel (`perform`), the pause after it lands (`arrive`), how long the badge is up before the gesture fires (`beat`), the pause after the action (`settle`), and how long a new note stays up before the demo acts on it (`hold`).

A single rate multiplier scales all of it, for re-timing a finished script. It is a last resort rather than the thing you author with.

## Gestures and the badge

The badge is a small dark chip beside the pointer naming the gesture in the host's own terms. The vocabulary is closed:

    move pointer   left click   right click   button down   drag   button up   scroll wheel

It is generated from the step, never authored. An author writes "patch this to that"; the runner expands it into move pointer, button down, move pointer, button up, and the badge names them one at a time. An eighth word is a discussion, not a new string.

Two notes on the expansion. Patching in Rack is a held drag from one port to another, not a click at each end, so it expands to a button-down and a button-up with a move between. And a knob is a drag rather than a wheel, so a value change shows as button down, a slow travel, button up — with the badge saying `drag`.

## The theatre

A widget added to the scene, above everything, taking no events. It draws the pointer, the badge, a ripple at each click, and a brief glow on the control being acted on. Rack draws its own cursor through the operating system, so the real one is hidden for the length of a take with a single GLFW call.

The card is one text place floating over the rack rather than docked beside it, because docking would take space from the thing being demonstrated. It is placed, not dragged: the runner knows every step's target in advance, works out the region the coming steps will touch, and gives the card a berth clear of it. The berth is chosen once per note and does not move while that note is up.

A note stays until the next note replaces it, so one note covers however many steps follow it.

## How a step reaches Rack

Through Rack's own event system. `APP->event->handleButton`, `handleHover`, `handleScroll` and `handleKey` are the functions Rack's own GLFW callbacks call, so a step driven through them does exactly what a person doing it would do. Menus, cable drags, module dragging, the module browser and text fields all work with no code of their own.

**Every step then asserts what it claimed to do.** After a patch step the cable exists between those two ports; after a set the parameter holds that value; after an add the module is on the rack. A click two pixels off a jack does nothing and would otherwise carry on silently to the end of the take, which is the one real objection to driving the interface this way. A failed assertion stops the run and names the step.

Direct calls remain available underneath — `Engine::setParamValue`, `Engine::addCable`, `RackWidget::addModule` — for anything injection turns out to handle badly. Nothing is expected to need them at the outset.

The real mouse is a hazard for the length of a take: if it moves, Rack delivers a hover to whatever it is over and the highlight follows it rather than the synthetic pointer. The overlay swallows real mouse movement while a demo is running.

## Control resolution

Steps address controls logically and resolve to screen coordinates at the last moment, so a script survives any window size, any zoom, and any rearrangement of the rack.

A module resolves by the name the script bound to it, through `RackWidget::getModule`. A parameter or a port resolves through the module widget's own `getParam`, `getInput` and `getOutput`, which give the widget and therefore its position. Positions are taken in scene coordinates at the moment of use, because the zoom widget between the rack and the scene means a module's box is not where it appears.

A control that is off screen is a script error rather than a position to point at. The `zoom` step exists so a script can frame what it is about to talk about; `RackScrollWidget::zoomToBound` does the work.

## Narration

Speech is rendered ahead of the take rather than spoken live, and the plugin renders it itself. Loading a script hashes every line of its prose, and any line with no audio file yet is rendered on the spot with the Mac's `say` in the voice the script names. `afinfo` then reports how long the file lasts. Both are already on the machine; there is no build step and no tool to run.

The duration is the point. A note holds for as long as its own sentence takes, and nothing is ever time-stretched: speech sets the floor and the rate multiplier squeezes only the silences around it. That number cannot be known without rendering first. Rendering also makes a second take of a script identical to the first, which speaking live would not.

Keying by a hash of the text means re-wording one note re-renders one file, and a script whose wording has not changed loads instantly. It also removes the failure that a separate rendering step invites, where a note is reworded, the render is forgotten, and the take speaks the old sentence with nothing on screen to say so.

A script is therefore self-contained: a markdown file and the plugin, with the audio a cache beside it that can be deleted at any time.

The plugin plays a fragment by spawning `afplay`, and knows when it ends from the measured duration rather than by waiting on the process. ScreenFlow records the computer audio, so the narration and the patch arrive in the take already mixed.

## Ducking

One recorded audio track means the balance cannot be fixed afterwards, so the runner sets it while it plays.

A script names the patch's master level in its header. For the length of each narration fragment the runner takes that parameter down by a stated number of decibels and puts it back after, moving it over a short ramp so the change is not a step. It is a parameter like any other, so this needs no mechanism beyond the one that performs a `set`.

Where a patch has no obvious master, the header may name any parameter, or none, in which case nothing is ducked.

## Recording

ScreenFlow, driven by hand. You press its hotkey, then Run.

Nothing needs trimming afterwards if the script opens and closes on a title card, which is the reason for the convention rather than a decoration. The plugin has no part in recording and makes no assumption that a recording is happening.

## The transport

One window, wide and short, dragged by its background and remembered where you put it. It is the same kind of widget as mpxChart's chart window.

Run, Stop, Restart, Back, Step, Rate and Close. Nothing pauses in flight, and **Stop is two-stage**: stopping a running demo leaves you standing on the step it reached with the rack as the demo built it, so you can step back or look at what it did; pressing Stop again puts your own patch back.

Stepping runs with every wait collapsed and says nothing. An author walking a script is reading, not listening, and a sentence per press would make stepping unusable.

## Stepping back, and the session guard

Every step is preceded by a snapshot of the whole patch, which is what makes stepping backwards as cheap as stepping forwards. Rack saves and loads a patch to a path, so a snapshot is a file in a scratch directory and a step back is a load.

The user's own patch is snapshotted the same way before a demo starts and restored when it ends, however it ends. Rack's autosave is left alone; the demo's patches are written to a directory of the plugin's own.

## Determinism

A script starts from a stated patch rather than from whatever was on screen, so the same run twice gives the same result.

The pointer's travel is timed off a wall clock and driven by frames when they arrive, but carried by a timer when they do not, so a window that stops receiving frames finishes the run without its in-between positions drawn rather than parking mid-step.

## Authoring

Scripts are written from spoken objectives, at whatever grain suits — an objective for a whole video, or a step at a time. The draft is run, the assertions confirm it did what it says, and the author steps through and corrects the wording.

The player is the authoring tool. There is no separate editor at the outset; the text is edited in the markdown file and reloaded, which is one keystroke away when the file is already open.

## Build order

Each phase stands alone and is worth having before the next exists.

1. **The overlay and the theatre** — the transport window, the synthetic pointer, the badge, the ripple, the card and its berths. Nothing is driven yet; a fixed list of coordinates proves the surface.
2. **Gestures and control resolution** — names to widgets to positions, and the seven gestures injected into Rack's event system, each asserting its result.
3. **The script and the runner** — the markdown parser, the step vocabulary, the pacing, snapshots and stepping.
4. **Narration** — hashing, `say`, `afinfo`, `afplay`, and ducking.
5. **The first video** — a script for mpxChart, which is the module that most needs showing rather than describing.
