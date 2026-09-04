# A first script

**Badges** off
**Captions** off
**Voice** Karen (Premium)
**Rate** 175
**Pacing** perform 1.1, arrive 0.9, beat 0.7, settle 0.6, hold 2.6

## say
This whole demonstration is a markdown file. One heading per step, and the prose underneath a heading is what gets said there.

## add Fundamental/VCO as osc
A step can put a module on the rack and give it a name. Every step after this one can address it as "osc".

## add Fundamental/VCF as filter

## zoom osc 1.6
Framing a module is a step of its own, so a script can point at something small without anybody having to zoom by hand first.

## set osc:#0 30%
Controls are addressed by name where a module gives them one, and by number where it does not. The value is a fraction of the control's own range, or a percentage, or a word like "centre" or "open".

## set osc:#0 70%

## zoom out

## patch osc:out:#0 -> filter:in:#0
A cable is a held drag from one jack to the other, because that is how one is really made. When the drag finishes, the step asks the engine whether the cable exists, and stops the demo if it does not.

## wait 2

## unpatch filter:in:#0
And off again: pulled from the jack and dropped on bare rack, which is what deletes one.

## say
Every step was snapshotted before it ran, so Back walks the patch backwards as easily as Run walks it forwards. Press Stop twice to put your own patch back.
