# Test Gear — instruments with no rack space

**Modules** gear = DreamerDevelopment/TestGear, clarity = DreamerDevelopment/Clarity, vco = Fundamental/VCO, lfo = Fundamental/LFO, tona = Instruo/tona, audio = Core/AudioInterface2
**Patch** ../patches/TestGearDemo.vcv
**Voice** Karen (Premium)
**Rate** 193
**Badges** off
**Captions** off
**Master** audio:Level
**Duck** 10 dB
**Before** clarity:Add and move = 0
**Pacing** perform 0.7, arrive 0.4, beat 0.3, settle 0.4, hold 0.4

## zoom out
Test Gear is a family of sixteen tools, which we call widgets, that clip onto any port to view, monitor, analyse or modify the signal there. You add one by right-clicking a terminal and choosing Widgets, at the top of the menu. The remarkable thing about them is that literally only the Test Gear module itself is taking up rack space. Here is a small patch with seven widgets already installed.

## zoom widget:vco:Sine 1.2
On the oscillator's sine output there is a frequency meter, reading the pitch in hertz. It can also show that pitch as a note, or the note a volt per octave cable is asking for. Clicking the chip at the top of a widget moves between the views it offers. Most of these widgets can do more than there is room for in this video; the manual has the rest.

## zoom widget:lfo:Sine 1.2
The LFO's sine output carries a voltmeter, showing the voltage at this moment. Its other view is the peak of the last quarter of a second.

## wait 0.5

## zoom widget:vco:Square 1.1
The square output has an oscilloscope on it.

## press widget:vco:Square/face@21,-8
The button at the bottom left freezes the trace so it can be looked at.

## wait 0.5

## press widget:vco:Square/face@21,-8
And starts it again.

## press widget:vco:Square/face@48,-8
AUTO sets both scales to fit what is coming in.

## wait 0.5

## press widget:vco:Square/face@50%,50%
Clicking anywhere on the scope's face hides the readout underneath it.

## wait 0.5

## press widget:vco:Square/face@50%,50%
And clicking again brings it back: volts per division on the left, the time base on the right.

## scroll widget:vco:Square@33,-11 up 2
Scrolling over either one changes it. This is the volts per division, so the trace grows.

## wait 0.5

## scroll widget:vco:Square@33,-11 down 2
And back.

## press widget:vco:Square/face@39,8
The yellow disc at the top left folds the scope away to a strip, for when you want it there but not in the way.

## wait 1

## press widget:vco:Square/face@39,8
And unfolds it again.

## say
A scope does a good deal more than this — triggering, coupling, an external trigger taken from another signal — and the documentation covers all of it.

## wait 0.5

## zoom widget:tona:out:Wavefold 1.1
The second oscilloscope is on the wave folder's output, and it is a different size from the first. A scope can be any shape you want it: drag an edge and the trace redraws to fit.

## wait 0.5

## zoom widget:tona:in:"1V/Oct" 1.2
This one is not a meter but a source: a steady voltage, clipped onto the pitch input, with its cable coming from Test Gear.

## scroll widget:tona:in:"1V/Oct"@15%,70% up 0.75
Scrolling the readout changes it — coarse to the left of the decimal point, fine to the right.

## say
A volt on that jack is an octave, so notice what one whole volt does.

## wait 1

## scroll widget:tona:in:"1V/Oct"@15%,70% down 0.75
And down an octave again, back where it started.

## wait 1

## press widget:tona:in:"1V/Oct"
A click on the face takes it out of circuit.

## wait 2

## press widget:tona:in:"1V/Oct"
And puts it back.

## wait 1

## zoom widget:tona:"Wavefold CV"/switch 1.2
And a switch, on an input that already has a cable in it. This one carries the LFO that is modulating the wave folder. Listen.

## press widget:tona:"Wavefold CV"/switch
Off.

## say
The cable is taken out of the port, and the folding stops with it. The stub left behind says where the cable went and what colour it was.

## wait 2

## press widget:tona:"Wavefold CV"/switch
On again.

## say
Exactly as it was, and the folding comes back.

## wait 2

## zoom widget:vco:in:"Frequency modulation"/av 1.2
On the oscillator's frequency modulation input there is an attenuverter — a widget that modifies a signal arriving at a port rather than reading one or making one. A widget can be clipped onto a jack that already has a cable in it: the engine sums everything arriving at an input, so this one works on the LFO already patched there. It scales what comes in, and it can turn it upside down.

## scroll widget:vco:in:"Frequency modulation"/av@15%,70% down 1.2
Down through nothing and out the other side. Notice that the direction of the oscillator's modulation changes: the modulation shrinks away, then comes back inverted, so the oscillator moves the opposite way to the LFO driving it.

## wait 1

## scroll widget:vco:in:"Frequency modulation"/av@15%,70% up 1.2
And back where it was.

## wait 1

## zoom vco start
Here is how to add a widget. Right-click on any terminal and choose Widgets, at the top of the menu, to see the list of everything that can be clipped on.

## menu vco:Triangle "Widgets…" "Scope"
This is the triangle output, which nothing is watching yet.

## press vco:Triangle@12,12
The new instrument follows the pointer until a click puts it down. Beside its own jack will do.

## wait 0.5
There is the triangle wave, on a jack that had nothing patched to it — a scope wakes an output up so that there is something to see.

## press widget:vco:Triangle@21,8
The red cross at its top left corner takes it off again.

## zoom out
Seven tools, taking up no rack space.
