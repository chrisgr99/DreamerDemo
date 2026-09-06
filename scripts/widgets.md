# Test Gear — instruments with no rack space

**Modules** gear = DreamerDevelopment/TestGear, clarity = DreamerDevelopment/Clarity, vco = Fundamental/VCO, lfo = Fundamental/LFO, tona = Instruo/tona, audio = Core/AudioInterface2
**Patch** ../patches/TestGearDemo.vcv
**Voice** Karen (Premium)
**Rate** 193
**Badges** off
**Captions** off
**Master** audio:Level
**Duck** 10 dB
**Before** clarity:Click to add and move cables = 0
**Pacing** perform 0.7, arrive 0.4, beat 0.3, settle 0.4, hold 0.4, tail 1.0

## zoom out
## wait 1.0

You can think of Test Gear as a workbench full of tools, available from the  right-click context menu on any terminal. 
We'll show you seven of them here.

## zoom widget:vco:Sine 1.2
Here, the frequency meter is clipped to the VCO's sine wave terminal, showing hertz. Click above the readout to see the signal as a musical note, or to display volt per octave as a note.

## zoom widget:lfo:Sine 1.2
Here, a voltmeter is clipped to the LFO's sine output, reading instantaneous voltage. Click above it to see the average over the last quarter second.

## wait 0.5

## zoom widget:vco:Square 1.1
Here, a scope clipped to the VCO square wave output.

## press widget:vco:Square/face@21,-8
Bottom left freezes the trace.

## wait 0.5

## press widget:vco:Square/face@21,-8
And starts it again.

## press widget:vco:Square/face@48,-8
AUTO scales both axes to the signal.

## wait 0.5

## press widget:vco:Square/face@50%,50%
Click on the face to toggle the display.

## wait 0.5
## press widget:vco:Square/face@50%,50%
Volts per division on the left, time base on the right.

## scroll widget:vco:Square@33,-11 up 2
Scroll either to adjust.

## wait 0.3

## scroll widget:vco:Square@33,-11 down 2
And back to the original scale.

## wait 0.3

## press widget:vco:Square/face@39,8
Click the yellow minimize button to shrink or restore it.

## wait 1

## press widget:vco:Square/face@39,8

## say
The scope also offers triggering, AC coupling and external triggering. See the manual for detail.

## wait 0.5

## zoom widget:tona:out:Wavefold 1.1
Here's a scope on the wave folder output. Notice it's wider than the other scope. Drag any edge to resize.

## wait 1.0

## zoom widget:tona:in:"1V/Oct" 1.2
Here is a DC voltage source injecting a 
steady signal into the pitch input.

## scroll widget:tona:in:"1V/Oct"@15%,70% up 0.75
Scroll the readout to change it. Coarse left of the decimal point, fine to the right.

## say
Increase the signal by 1.0, raising it by one octave.

## wait 1

## scroll widget:tona:in:"1V/Oct"@15%,70% down 0.75
And back.

## wait 1

## press widget:tona:in:"1V/Oct"
A click disables the widget. Its one volt is removed, and the pitch falls an octave.

## wait 0.5

## press widget:tona:in:"1V/Oct"
Enabled again.

## wait 1

## zoom widget:tona:"Wavefold CV"/switch 1.2
This is a switch, on an input with a connected cable: the LFO modulating the wave folder.

## press widget:tona:"Wavefold CV"/switch
Click to turn it off.

## say
The cable becomes disconnected but still shows as a stub on the terminal. Notice how the wave folder modulation stops.

## wait 3

## press widget:tona:"Wavefold CV"/switch


## say
Click again to restore it.

## wait 3

## zoom widget:vco:in:"Frequency modulation"/av 1.2
Here is an attenuverter on the frequency modulation input. It can scale and even invert the LFO signal. Widget outputs are added to any signal on the port.

## scroll widget:vco:in:"Frequency modulation"/av@15%,70% down 1.2
Turn it down through zero and then negative. The modulation falls away, then inverts.

## wait 2

## scroll widget:vco:in:"Frequency modulation"/av@15%,70% up 1.2
And back to unity.

## wait 2

## zoom vco start
It's easy to add a widget. Right-click on any terminal and choose Widgets from the top of the menu.

## menu vco:Triangle "Widgets…" "Scope"
Let's add a scope to The VCO triangle wave output.

## wait 2

## press vco:Triangle@12,12
A new widget follows the pointer until a click places it.

## wait 2

## press widget:vco:Triangle@21,8
To remove it again, Click the red X at the upper left.

## zoom out
There you have it, Seven useful widgets, taking up no rack space. Install the Test Gear module and you'll have 16 widgets at your fingertips.
