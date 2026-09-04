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

    say                            nothing happens; the prose is the step
    wait <seconds>                 let the patch play
    point <target>                 go there and say the note; touch nothing
    press <target>                 left click it
    click <target>                 the same word, for a control that is not a button
    right <target>                 right click it, which opens its menu
    set <target> <value>           a fraction, a percentage, or a word
    scroll <target> [up|down]
    patch <target> -> <target>     either way round
    unpatch <target>               pull the cable off and drop it
    menu <target> <item>           right click, then choose that item
    move <name> <hp> <rows>        drag a module by its panel
    zoom <name> [factor]           frame a module; `zoom out` frames the whole rack
    pan <name>                     centre on a module without changing how close it is
    pan <hp> <rows>                move the view by that much
    open <patch file>              load a patch
    add <Plugin/Model> as <name>   add a module and bind a name to it
    key <name>                     press a key where the pointer is standing

`window` addresses the floating window at the front that belongs to somebody else — a window a module opened, such as the chart's. It has no controls a script could name, because it is not a module, so `window:close` and `window:close-right` reach the corners where a close control lives. Shutting a window by its own cross is what a person does; pressing the button that opened it a second time is not.

A target is `name` for the module itself or `name:control` for one of its controls, where the control is a parameter's or a port's own name. `in:` and `out:` in front of it force the side, and `#3` addresses one by number for a module that names nothing.

Every step carries its own pacing, and the script's defaults supply whatever a step leaves out: how long the pointer takes to travel (`perform`), the pause after it lands (`arrive`), how long the badge is up before the gesture fires (`beat`), the pause after the action (`settle`), and how long a new note stays up before the demo acts on it (`hold`).

A single rate multiplier scales all of it, for re-timing a finished script. It is a last resort rather than the thing you author with.

## Gestures and the badge

The badge is a dark chip beside the pointer naming the gesture in the host's own terms. It is set at caption size rather than tooltip size — it is read at a glance while the pointer is the thing being watched, from across a room or through a magnified view — and everything about it is a fraction of that one number.

It can be turned off for a whole script. Once the narration speaks a gesture as well as writing it, the badge is a caption repeating the voice. The pacing does not change either way, so the same script runs to the same length with the badge up or down and the two can be judged against each other.

The vocabulary is closed:

    move pointer   left click   right click   button down   drag   button up   scroll wheel

It is generated from the step, never authored. An author writes "patch this to that"; the runner expands it into move pointer, button down, move pointer, button up, and the badge names them one at a time. An eighth word is a discussion, not a new string.

**A click is held, not instantaneous.** A press and a release inside one frame are invisible to anything watching a parameter for an edge — and a momentary button is exactly that, rising on the press and falling on the release, so a module stepping once per frame sees it at rest both times and never learns it was pressed. The button goes down when the gesture starts and comes up when it ends, which is also what a real click does.

Two notes on the expansion. Patching in Rack is a held drag from one port to another, not a click at each end, so it expands to a button-down and a button-up with a move between. And a knob is a drag rather than a wheel, so a value change shows as button down, a slow travel, button up — with the badge saying `drag`.

## The theatre

A widget added to the scene, above everything, taking no events. It draws the pointer, the badge, a ripple at each click, and a brief glow on the control being acted on. Rack draws its own cursor through the operating system, so the real one is hidden for the length of a take with a single GLFW call.

The card is one text place floating over the rack rather than docked beside it, because docking would take space from the thing being demonstrated. It is placed, not dragged: the runner knows every step's target in advance, works out the region the coming steps will touch, and gives the card a berth clear of it. The berth is chosen once per note and does not move while that note is up.

A note stays until the next note replaces it, so one note covers however many steps follow it.

**Nothing a viewer must be told goes through the card.** Scripts are normally run with captions off, because the speech carries the words — so a failure written on the ordinary card is a failure nobody hears about, and the run simply stops looking as though it did nothing. A message that is not narration ignores the captions switch without changing it, so the next run is still captioned the way its script asked.

**The card can be turned off, and often will be.** Experiment with the same system in DreamRack settled it: a demo reads better with the captions and the badges both down and the speech carrying the action. Written and spoken narration are the same words twice, and the eye leaves the thing being demonstrated in order to read. The card stays in the design because a demo without sound needs it, and because it is what an author reads while stepping through a script in silence.

Turning either off changes nothing about the pacing. A note's hold is how long its own sentence takes, spoken or not, so the same script runs to the same length with them up or down and the two can be judged against each other.

## How a step reaches Rack

Through Rack's own event system. `APP->event->handleButton`, `handleHover`, `handleScroll` and `handleKey` are the functions Rack's own GLFW callbacks call, so a step driven through them does exactly what a person doing it would do. Menus, cable drags, module dragging, the module browser and text fields all work with no code of their own.

**Every step then asserts what it claimed to do.** After a patch step the cable exists between those two ports; after a set the parameter holds that value; after an add the module is on the rack. A click two pixels off a jack does nothing and would otherwise carry on silently to the end of the take, which is the one real objection to driving the interface this way. A failed assertion stops the run and names the step.

**A click checks what it is about to land on.** A press goes to whatever is topmost at that point, and a window over the rack — the chart's own, a menu, this plugin's transport — is topmost. The press then does something else entirely, or nothing at all, while the demo carries on believing it pressed a button. So before every click the runner asks Rack what it believes is under the pointer, and a control that is covered stops the run and says so rather than being silently missed.

**Nor is a cable's removal.** How far a cable must be dragged before Rack lets go of it, and what counts as somewhere it will accept, is not knowable from a script: the nearest bare rack can be a long way from the jack, and a drop that lands on anything at all puts the cable back. So the pointer shows the pull and the removal is done through the port's own delete, with its undo entry — the same bargain as a value.

**One thing is deliberately not injected: a value.** How far a knob turns for a given movement is the knob's own business — its range, its sensitivity, whether it snaps — so a drag long enough to reach a value on one control overshoots on the next, and neither distance is knowable from a script. A `set` step writes the value through the parameter and lets the pointer show a drag over the top of it. This is the one place where the theatre and the behaviour are different things on purpose, and it is why a `set` can be checked against the value it asked for.

Everything else goes through the event system. Direct calls remain available underneath for anything injection turns out to handle badly.

The real mouse is a hazard for the length of a take: if it moves, Rack delivers a hover to whatever it is over and the highlight follows it rather than the synthetic pointer. The overlay swallows real mouse movement while a demo is running.

**It is not on screen while a demo runs.** The transport is the author's window, not the viewer's: nobody watching a video should see the thing driving it, and while it was visible the synthetic pointer could walk onto it and drag it about. It is hidden rather than moved, so it draws nothing and receives nothing. Escape still stops a run and hands the patch back, which is the one control a take needs.

## Never rearrange the scene while Rack is walking it

A click on the transport arrives in the middle of Rack's own walk of the scene's children, dispatching that event. Anything done from inside a button press that removes or re-adds a child is rearranging that list underneath the walk — and the press then arrives somewhere else entirely, such as a jack behind the window, which picks up a cable instead of starting the demo.

So bringing the pointer and the card to the front is asked for and done on the next frame. And the transport is not brought to the front at all when it is already open: it is visible, which is enough, and the alternative was a whole class of this fault for no gain.

## The camera

A zoom or a pan is not something the pointer does, and it is not a cut. It eases from where the view is to where it should be, and it starts **as its step's note goes up** rather than after the note has been read — a demo that talks about a module for four seconds and only then brings it into view has described something the viewer cannot see.

So a camera step runs on its own clock beside the narration, and the step's prose is what is said while the move happens. Nothing waits for it.

**The pointer travels with it**, on a zoom. Zooming to a module means "look at this", and the pointer is what says which thing is being looked at, so it walks there over the same seconds and arrives as the view settles. Its destination is asked for again every frame, because the module is travelling across the screen while the camera closes on it — a destination worked out once, before the move, would be where the module used to be. A pan does not do this: panning is framing, not attention.

Zoom is interpolated geometrically: halfway between one and four is two, not two and a half. A linear ride between two zoom levels rushes at one end and crawls at the other.

**The view is held as a place and a closeness**, not as Rack's scroll offset. Rack keeps a pixel offset and a zoom, and offers a grid offset measured from an origin constant; neither moves smoothly, because a pixel offset means something different at every zoom and the grid one carries that constant. What a camera has is a point it is looking at and how close it is, and those two are independent — so those are what get interpolated, and the offset is worked out from them on every frame with Rack's own arithmetic: the point times the zoom, less half the viewport. Panning moves the point and leaves the zoom alone.

## Control resolution

Steps address controls logically and resolve to screen coordinates at the last moment, so a script survives any window size, any zoom, and any rearrangement of the rack.

A module resolves by the name the script bound to it, through `RackWidget::getModule`. A parameter or a port resolves through the module widget's own `getParam`, `getInput` and `getOutput`, which give the widget and therefore its position. Positions are taken in scene coordinates at the moment of use, because the zoom widget between the rack and the scene means a module's box is not where it appears.

A control that is off screen is a script error rather than a position to point at. The `zoom` step exists so a script can frame what it is about to talk about; `RackScrollWidget::zoomToBound` does the work.

## Narration

Speech is rendered ahead of the take rather than spoken live, and the plugin renders it itself. Loading a script hashes every line of its prose, and any line with no audio file yet is rendered on the spot with the Mac's `say` in the voice the script names. `afinfo` then reports how long the file lasts. Both are already on the machine; there is no build step and no tool to run.

Rendering happens when a script is **loaded**, not when it is run, so Run is always immediate and a reworded note cannot reach a take still speaking the old words. A voice the machine does not have renders nothing, and would do it in silence — every note falling back to its written hold with no sign of why — so a missing voice is checked for and said out loud in the log.

The duration is the point. A note holds for as long as its own sentence takes, and nothing is ever time-stretched: speech sets the floor and the rate multiplier squeezes only the silences around it. That number cannot be known without rendering first. Rendering also makes a second take of a script identical to the first, which speaking live would not.

The key is a hash of the words, **the voice and the speed** — all three, because all three change what comes out of the loudspeaker. Hashing the words alone meant that changing the voice or the rate re-rendered nothing at all, since every line was already there, and the demo went on speaking in the old voice with no sign of why. So re-wording one note re-renders one file, changing the voice re-renders the script, and a script whose wording has not changed loads instantly.

The default rate is a tenth faster than a plain reading. Speech at a conversational pace sounds slow against a demonstration, where the picture is already moving; a tenth over is brisk without being hurried, and nothing is ever time-stretched to reach it. It also removes the failure that a separate rendering step invites, where a note is reworded, the render is forgotten, and the take speaks the old sentence with nothing on screen to say so.

A script is therefore self-contained: a markdown file and the plugin, with the audio a cache beside it that can be deleted at any time.

The plugin plays a fragment by spawning `afplay`, and knows when it ends from the measured duration rather than by waiting on the process — so a frame is never blocked by a program starting. ScreenFlow records the computer audio, so the narration and the patch arrive in the take already mixed.

A note's hold is then whichever is longer: the number the script gave it, or however long the line actually takes to say. A demo that stops, stops talking.

## Ducking

One recorded audio track means the balance cannot be fixed afterwards, so the runner sets it while it plays.

A script names the patch's master level in its header, and what it should fall to. **Where that level rests is captured once, when the run starts** — not each time the voice begins. Capturing it per line meant a duck landing on an already-ducked value took that as the resting level: multiply a level by a third a few times and it reaches silence, with nothing left that knows what to put back. Once per run cannot compound, one restore puts it right however many lines were spoken, and it is put back unconditionally rather than only when the runner believes it is down — a level left low is the one failure a viewer cannot diagnose, because the patch simply makes no sound and nothing on screen says why. A master already at the bottom is not ducked at all.

The runner pulls that parameter down as a line begins and puts it back the moment the voice stops rather than at the end of the step — a note holds for as long as its sentence and often longer, and the patch should be at full level for the remainder rather than under a voice that has finished. It is a parameter like any other, so this needs no mechanism beyond the one that performs a `set`.

Where a patch has no obvious master, the header may name any parameter, or none, in which case nothing is ducked.

`Before` is a list of `target = value` pairs put in place at the start of a run, silently and at once — the conditions a script needs rather than anything it shows. Turning off a host feature that would fight the demo is setup, and watching a pointer travel across the screen to do it cost two and a half seconds before the first word was said.

`Duck` is in **decibels**, because that is the only unit in which "duck it a bit" means the same thing on two different faders — a fraction of a parameter's own range says nothing about how much quieter anything got. A fader that displays decibels is asked for its current reading less the duck, which is right whatever curve it uses underneath; anything else is treated as a linear gain and scaled, which is what a level control is even when it does not say so. Ten decibels by default.

The header therefore carries `Voice`, `Rate`, `Master` and `Duck` alongside the pacing, because the voice and the words it will speak are one decision and belong in one place.

## Recording

ScreenFlow, driven by hand. You press its hotkey, then Run.

Nothing needs trimming afterwards if the script opens and closes on a title card, which is the reason for the convention rather than a decoration. The plugin has no part in recording and makes no assumption that a recording is happening.

## The transport

One window, wide and short, dragged by its background and remembered where you put it. It is the same kind of widget as mpxChart's chart window.

Run, Stop, Restart, Back, Step, Rate and Close. Nothing pauses in flight, and **Stop is two-stage**: stopping a running demo leaves you standing on the step it reached with the rack as the demo built it, so you can step back or look at what it did; pressing Stop again puts your own patch back.

Stepping runs with every wait collapsed and says nothing. An author walking a script is reading, not listening, and a sentence per press would make stepping unusable.

## Stepping back, and the session guard

Every step is preceded by a snapshot of the whole patch, which is what makes stepping backwards as cheap as stepping forwards. Rack saves and loads a patch to a path, so a snapshot is a file in a scratch directory and a step back is a load.

The user's own patch is snapshotted the same way before a demo starts, and handed back on an explicit act: Stop pressed a second time, loading another script, or closing the transport.

**Not on the last step.** A script that reaches its end leaves the rack it built standing. A take that cut back to the viewer's own patch on the final frame would be unusable, and an author wants to look at what the demo made. So the end of a script puts the window into the same state as a Stop — the button reads Reset — and one press hands the patch back. A failed step lands in the same place.

**Not on exit either, and nor is anything else.** A remove event says nothing about why it fired: it fires when the window is closed, and again when Rack destroys the scene at quit. On an exit the rack, the engine and every module have already gone, so anything that reaches for one of them is reading freed memory — loading a patch, asking the rack for half-made cables, handing a ducked level back through a module's parameter, injecting a mouse release. All of those are correct on a close and fatal on an exit.

A deliberate close is the only case in which the application is known to be alive, so it is the only case that does any of them. On the way out the only thing done is silencing the narration, because that is a separate process and would otherwise carry on talking after Rack has gone. The patch history is what recovers a rack the demo was still holding.

The cost is a small window: quitting in the middle of a spoken line leaves the master ducked, and Rack's autosave may keep it. The window is a second or two, since the level is handed back the moment each line ends.

Rack's autosave is left alone; the demo's patches are written to a directory of the plugin's own.

## The patch history

Rack keeps one autosave and overwrites it every fifteen seconds, so a crash or a mistake costs whatever was there — there is nothing older to go back to. So a dated copy of the patch is written every time the patch changes, with its report beside it, and the newest four hundred are kept. That is several hours of continuous work, and a patch file is small enough that the whole history is smaller than one screenshot.

It is a safety net rather than version control: no names and no messages, just the rack as it stood at a moment, and a menu that lists them newest first and asks before replacing what is there.

## Reading somebody else's patch

A patch file records a cable as a pair of port numbers, and what those ports are called lives only in the running program, where each module declares them with configParam, configInput and configOutput. So two files are written that turn the numbers back into names.

**The port index** builds one of every module of every installed plugin and writes what each of its controls is called, indexed by exactly the number a patch file records, together with each plugin's licence and source URL — which decides whether a question about a module's behaviour can be answered by reading its code. It is written a line at a time and flushed as it goes, and each model's name goes down before that model is built, so a plugin that misbehaves in its constructor is named by the last line in the file rather than losing the whole run.

**The patch report** describes the rack as it stands, in names rather than numbers: the modules, their settings with units, every cable written as one named port to another, and which inputs have nothing patched into them. That last line is the one that finds a patch making no sound. It rewrites itself every fifteen seconds whenever it has something different to say.

## Determinism

A script starts from a stated patch rather than from whatever was on screen, so the same run twice gives the same result.

The pointer's travel is timed off a wall clock and driven by frames when they arrive, but carried by a timer when they do not, so a window that stops receiving frames finishes the run without its in-between positions drawn rather than parking mid-step.

## Authoring

Scripts are written from spoken objectives, at whatever grain suits — an objective for a whole video, or a step at a time. The draft is run, the assertions confirm it did what it says, and the author steps through and corrects the wording.

The player is the authoring tool. There is no separate editor at the outset; the text is edited in the markdown file and reloaded, which is one keystroke away when the file is already open.

## Build order

Each phase stands alone and is worth having before the next exists.

1. **The overlay and the theatre** — the transport window, the synthetic pointer, the badge, the ripple, the card and its berths. *Done.*
2. **Gestures and control resolution** — names to widgets to positions, and the seven gestures injected into Rack's event system, each asserting its result. *Done.*
3. **The script and the runner** — the markdown parser, the step vocabulary, the pacing, snapshots and stepping. *Done.*
4. **Narration** — hashing, `say`, `afinfo`, `afplay`, and ducking. *Done.*
5. **The first video** — a script for mpxChart, which is the module that most needs showing rather than describing. *Written; the words are what gets revised now.*

The scripts themselves are kept in `scripts/` in this repository and copied into `DreamerDemo/scripts` in Rack's user folder, which is where the plugin reads them. The repository copy is so a script's wording has a history; the working copy is the one being edited.
