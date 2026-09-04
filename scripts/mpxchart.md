# mpxChart — playing the changes

**Modules** chart = DreamerMPX/mpxChart, arp = HamptonHarmonics/Arp, audio = Core/AudioInterface2, clarity = DreamerDevelopment/Clarity
**Voice** Karen (Premium)
**Rate** 193
**Badges** off
**Captions** off
**Master** audio:Level
**Duck** 10 dB
**Before** clarity:Add and move = 0
**Pacing** perform 0.7, arrive 0.5, beat 0.4, settle 0.5, hold 0.5

## zoom chart 1.4
This is mpxChart. It loads iReal Pro charts and playlists — jazz lead sheets, as a musician would be handed one — plays them in time, and puts the chord of the moment out as a polyphonic volt per octave cable. Let's start it.

## press chart:Play

## say
The face shows the tune, the key, which bar is sounding, and the chord under way at this moment.

## press chart:Open
Everything else about the chart is read in a window of its own.

## say
Four bars to a line, always, so the barlines run straight down the page, and a section always starts a fresh line.

## say
The whole window is one control. Drag it wider and the chords, the marks and the spacing all grow together, because every one of them is a fraction of the width of a bar.

## say
The measure being played is filled in as it goes. Clicking any bar sends the play head there, so you can jump about the form while it is running.

## say
Clicking a section letter chooses that section, and an orange line above its bars shows which one is chosen. The chart then plays that section over and over instead of the whole form.

## point window
Escape shuts it, from anywhere.

## key escape

## press chart@50%,12%
The title of the tune is also the way to another one. Pressing it opens the playlists that have been loaded, and the charts in each of them.

## wait 2

## key escape

## zoom chart 0.45

## point chart:"Chord tones"
The chord sounding now leaves here as polyphonic volt per octave, one channel to a note. Beside it are the root on its own, and the notes of the key as a scale, for a quantiser.

## point chart:"MPX note out"
And this one carries MPX, Modular Polyphonic Expression: a protocol Dreamer Development is building to pass music between a family of MPX modules.

## say
Every note on it keeps its own pitch, level, duration, pan, bend, pressure and timbre — what MIDI Polyphonic Expression does for a keyboard, and rather more of it.

## say
It carries the harmony as harmony too: the chord named, its root and quality, the key it sits in, how many beats of it are left, and what comes next. A pitch cable can send the notes of a chord. It cannot say which chord they are.

## unpatch arp:in:"1V/oct"

## patch chart:out:"Chord tones" -> arp:in:"1V/oct"
Into an ordinary arpeggiator, which knows nothing about any of this. It sees four voltages and plays them in turn, and the chart changes them underneath it as the form goes by.

## wait 8

## press chart:Play
Play is a transport, not a mute. Nothing advances now that it is off, but the harmony is still being published, so everything downstream knows the chord you stopped on.
