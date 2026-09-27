# Snap to rows

**Patch** ../patches/RowsDemo.vcv
**Modules** clarity = DreamerDevelopment/Clarity, vco = Fundamental/VCO, scope = Fundamental/Scope, delay = Fundamental/Delay
**Badges** off
**Captions** off
**Voice** Karen (Premium)
**Rate** 193
**Pacing** perform 1.1, arrive 0.9, beat 0.7, settle 0.6, hold 2.2
**Rows** 2
**Row** 0

## point clarity:snap
Snap to rows sets the zoom so that whole rows fill the window, and holds the view on them. Two rows here.

## scroll scope down 2
The wheel moves a row at a time, and keeps moving for as long as it is turned.

## key up
The up and down arrows do the same, a row a press.

## key up

## key cmd+up
Command with them changes how many rows are shown, one to five. So does the gesture Rack zooms with.

## key cmd+down

## row 0

## patch vco:sine -> scope:ch 1
Both rows are in the window at once, so a cable between them is one movement.

## wait 2
