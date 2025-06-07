*  testcoco.asm
*  by Richard Cavell
*
*  This is used to test BASICloader.
*  Assume the least powerful Coco (Coco 1 with 4 kilobytes of RAM, no extended BASIC)
*
*  June 2025

start	ldx 1024	; Start of text screen
	lda 'A'
	sta ,x          ; Put an A at the top left corner
	clra
	rts
