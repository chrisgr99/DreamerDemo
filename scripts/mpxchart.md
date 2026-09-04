# mpxChart — playing the changes

**Modules** chart = DreamerMPX/mpxChart, arp = HamptonHarmonics/Arp, audio = Core/AudioInterface2
**Voice** Karen (Premium)
**Rate** 193
**Badges** off
**Captions** off
**Master** audio:Level
**Duck** 10 dB
**Pacing** perform 0.7, arrive 0.5, beat 0.4, settle 0.5, hold 0.5

## zoom chart 1.4
This is mpxChart. It reads a chord chart — a jazz lead sheet, as a musician would be handed one — and plays the harmony into the rest of the rack. Let's start it.

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

## wait 2

## press window:close
A cross at either end of the title bar shuts it.

## zoom out

## point chart:"Chord tones"
Rack has no way to carry a chord as a chord. So the chart also speaks two languages that every rack already understands: the chord's tones as a polyphonic pitch cable, and the notes of the key as a scale.

## unpatch arp:in:"1V/oct"

## patch chart:out:"Chord tones" -> arp:in:"1V/oct"
Into an ordinary arpeggiator, which knows nothing about any of this. It sees four voltages and plays them in turn, and the chart changes them underneath it as the form goes by.

## wait 8

## press chart:Play
Play is a transport, not a mute. Nothing advances now that it is off, but the harmony is still being published, so everything downstream knows the chord you stopped on.
