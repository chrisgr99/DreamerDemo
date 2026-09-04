# mpxChart — playing the changes

**Modules** chart = DreamerMPX/mpxChart, arp = HamptonHarmonics/Arp, audio = Core/AudioInterface2
**Voice** Karen (Premium)
**Rate** 175
**Badges** off
**Captions** off
**Master** audio:Level
**Duck** 35%
**Pacing** perform 1.2, arrive 1.0, beat 0.7, settle 0.7, hold 2.4

## zoom chart 1.4
This is mpxChart. It reads a chord chart — a jazz lead sheet, as a musician would be handed one — and plays the harmony into the rest of the rack.

## say
The face shows the tune, the key, which bar is sounding, and the chord under way at this moment. Everything else about the chart is read in a window of its own.

## point chart:Tempo
Tempo, when nothing is clocking it. The green figure above the knob is the rate actually in force, so it tells the truth whether it is coming from this knob or from a clock cable.

## set chart:Tempo 45%

## press chart:Play
Play is a transport, not a mute. Nothing advances while it is off, but the harmony keeps being published, so whatever is downstream still knows the chord you stopped on.

## wait 6

## zoom out

## point chart:"Chord tones"
Rack has no way to carry a chord as a chord. So the chart also speaks two languages that every rack already understands: the chord's tones as a polyphonic pitch cable, and the notes of the key as a scale.

## unpatch arp:in:"1V/oct"

## patch chart:out:"Chord tones" -> arp:in:"1V/oct"
Into an ordinary arpeggiator, which knows nothing about any of this. It sees four voltages and plays them in turn, and the chart changes them underneath it as the form goes by.

## wait 8

## press chart:Open
The chart itself opens in a window of its own. Four bars to a line, always, so the barlines run down the page — and the whole thing is one control: drag the window wider and the music grows with it.

## wait 10

## press chart:Play
Stopping leaves the harmony where it stands.

## say
The measure playing is filled in as it goes, and clicking a section letter chooses that section — every occurrence of it, which is what a musician means by "just the A section".
