*  testcoco.asm
*  by Richard Cavell
*
*  This is used to test BASICloader.
*  Assume the least powerful Coco (Coco 1 with 4 kilobytes of RAM, no extended BASIC, no disk drive)
*
*  This file is intended to be input to XRoar
*
*  June 2025

	ORG $3F00

start:	ldx #$6f 	; DEVNUM
        lda 0           ; 0 means the screen
        sta ,x

        ldx #msg
loop:
	lda ,x+
        beq end
	jsr [$A002]     ; CHROUT
        bra loop
end:	clra
	rts

msg:	FCN "TEST succeeded"
	END
