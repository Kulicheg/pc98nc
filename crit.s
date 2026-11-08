/*
 * INT 24h critical error. DOS otherwise prints Abort/Retry/Fail and
 * Abort ends the program. Fail the call so INT 21h returns an error.
 * AH bit 3 means Fail is allowed. If it is not, Ignore when that is
 * allowed, otherwise Fail anyway.
 */
	.arch	i8086, nojumps
	.code16

	.section .data
old24_off:
	.word	0
old24_seg:
	.word	0
	.global	crit_hit
crit_hit:
	.byte	0

	.section .text
app_ds:
	.word	0

	.global	crit_fail
crit_fail:
	pushw	%ds
	pushw	%bx
	movw	%cs:app_ds, %bx
	movw	%bx, %ds
	movb	$1, crit_hit
	popw	%bx
	popw	%ds
	testb	$0x08, %ah
	jnz	crit_do_fail
	testb	$0x20, %ah
	jz	crit_do_fail
	xorb	%al, %al
	iret
crit_do_fail:
	movb	$3, %al
	iret

	.global	dos_crit_on
dos_crit_on:
	pushw	%bx
	pushw	%es
	movw	%ds, %bx
	movw	%bx, %cs:app_ds
	movb	$0x35, %ah
	movb	$0x24, %al
	int	$0x21
	movw	%bx, old24_off
	movw	%es, old24_seg
	popw	%es
	pushw	%ds
	movw	%cs, %bx
	movw	%bx, %ds
	movw	$crit_fail, %dx
	movb	$0x25, %ah
	movb	$0x24, %al
	int	$0x21
	popw	%ds
	popw	%bx
	ret

	.global	dos_crit_off
dos_crit_off:
	pushw	%bx
	movw	old24_seg, %bx
	orw	%bx, %bx
	jz	crit_off_done
	pushw	%ds
	movw	old24_off, %dx
	movw	%bx, %ds
	movb	$0x25, %ah
	movb	$0x24, %al
	int	$0x21
	popw	%ds
	movw	$0, old24_seg
crit_off_done:
	popw	%bx
	ret
