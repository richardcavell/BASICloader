*  testcoco.asm
*  by Richard Cavell
*
*  This is used to test BASICloader.
*  Assume the least powerful Coco (Coco 1 with 4 kilobytes of RAM, no extended BASIC, no disk drive)
*
*  This program is intended to be input to XRoar
*
*  June 2025

	ORG 0x0F00

start:	ldx #$6f 	; DEVNUM
        lda #0          ; 0 means the screen
        sta ,x

        ldx #msg
loop:
	lda ,x+
        beq end
	jsr [$A002]     ; CHROUT
        bra loop
end:	lda #0
	rts

msg:	FCN "TEST SUCCEEDED\n"
	END
