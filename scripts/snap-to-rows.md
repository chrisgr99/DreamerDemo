# Snap to rows

**Patch** ../patches/RowsDemo.vcv
**Modules** clarity = DreamerDevelopment/Clarity, vco = Fundamental/VCO, vcf = Fundamental/VCF
**Badges** off
**Captions** off
**Voice** Karen (Premium)
**Rate** 193
**Pacing** perform 1.1, arrive 0.9, beat 0.7, settle 0.6, hold 2.4

## rows 2

## pan clarity

## row 0

## point clarity:snap
A rack is a wall of rows, and the window usually shows the bottom of one row and the top of the next. Snap to rows sets the zoom so that a whole number of rows fills the window, with a little of the row above and below showing, and holds the view there.

## key down
The down arrow moves a row.

## key down

## key up
The up arrow moves back.

## key up

## key cmd+up
Command with the arrows changes how many rows are on show, from one to five. The zoom follows the count, and whatever the pointer is over stays under the pointer.

## key cmd+down

## patch vco:sine -> vcf:audio
With two rows in the window, a cable goes from one row to the next without any scrolling.

## wait 2
