These are patches for Tektronix "Field Adjustment Software", to enable the use of different GPIB cards than the hardcoded defaults.

Patches may be applied with either
- [xdelta3](http://xdelta.org/)
- [pycrk](https://github.com/pR0Ps/pycrk)

## Background


The field adjustment software needs either an ISA,  NI GPIB-PCII or PCIIA card, which are getting hard to come by. These cards are based on a NEC7210-compatible IC which implements a set of 8 registers. One difference between PCII and PCIIA cards is the mapping of these registers in the x86 IO address space. Otherwise they behave pretty much the same, as far as the field adjustment software (FAS) is concerned.

Now, as it turns out, many ICs have implented a 7210-compatible interface or just cloned it altogether. Anything NAT4882-based necessarily has a 7210 mode, and this means many many ISA cards could *potentially* be used with the FAS.
The caveat of course being that the FAS needs to be modified to use the proper IO address mapping.



### IO address map
Here's a table that hopefully explains better than me.
```
index : 7210 register #
name : 7210 register name
*_mapping : IO port used to access the register on a certain card type (AT is GPIB-AT, not sure about GPIB-AT/TNT)

index	name	PC2_mapping	PC2A_mapping	AT_mapping
0	CDOR	02B8	02E1	02C0
1	IMR1	02B9	06E1	02C2
2	IMR2	02BA	0AE1	02C4
3	SPMR	02BB	0EE1	02C6
4	ADMR	...
5	AUXMR	...
6	ADR	...
7	EOSR	...
```

Here's the relevant section of code in the FAS that generates the IO port mappings:

```
;Dissassembly from TDS700.exe near file offset 0x346CD
;
;This code path parses the "GPIBn=PC2A [dmach] [irq] [DIPswitch]" environment variable
;(set in ADJ700.bat)
;
;gpib_cardID is "n" in "set GPIBn=...", so 0 or 1.

;Notice this pattern:
;	lea	ax, [di+2E1h]
;which does "ax = di + 0x2E1", where di is generated based on the DIP switch setting.
;We recognize 2E1 as the base address for register CDOR on the PCIIA cards !

3B14:017F	mov	si, [bp+gpib_cardID]
3B14:0182	shl	si, 1
3B14:0184	mov	es, seg_77546
3B14:0188	mov	di, es:[si+big.GPIB_DIPswitch]
3B14:018D	mov	cl, 0Dh	; shift DIPswitch setting to set address bits A14..A13
3B14:018F	shl	di, cl
3B14:0191	lea	ax, [di+2E1h]
3B14:0195	mov	es, seg_77548
3B14:0199	mov	es:[si+big.DIR_CDOR], ax
3B14:019E	lea	ax, [di+6E1h]
3B14:01A2	mov	es, seg_7754A
3B14:01A6	mov	es:[si+big.ISMR1], ax
3B14:01AB	lea	ax, [di+0AE1h]
3B14:01AF	mov	es, seg_7754C
3B14:01B3	mov	es:[si+big.ISMR2], ax
; ... and so on.
```

### Original decompiled code
Maybe this decompiled code will help understand what happens there :

```
  iVar6 = strcmp_0d84(..., "PC2A");
  if (iVar6 == 0) {
                    /* PC2A addresses */
    p_bigstruct = (big_struct *)(iVar3 * 2);
    iVar6 = p_bigstruct->GPIB_DIPswitch << 0x0d;
    p_bigstruct->gpib_CDOR = iVar6 + 0x2e1;
    p_bigstruct->gpib_IMR1 = iVar6 + 0x6e1;
    p_bigstruct->gpib_IMR2 = iVar6 + 0xae1;
    p_bigstruct->gpib_SPMR = iVar6 + 0xee1;
    p_bigstruct->gpib_ADMR = iVar6 + 0x12e1;
    p_bigstruct->gpib_AUXMR = iVar6 + 0x16e1;
    p_bigstruct->gpib_ADR = iVar6 + 0x1ae1;
    iVar6 = iVar6 + 0x1ee1;
  }
  else {
                    /* PC2 addresses */
    tmp2 = (big_struct *)(iVar3 * 2);
    uVar5 = *(undefined2 *)0x2e78;
    tmp2->gpib_CDOR = tmp2->GPIB_DIPswitch << 3;
    tmp2->gpib_IMR1 = *(int *)(iVar3 * 2 + 0x4a) + 1;
    tmp2->gpib_IMR2 = *(int *)(iVar3 * 2 + 0x4a) + 2;
    tmp2->gpib_SPMR = *(int *)(iVar3 * 2 + 0x4a) + 3;
    tmp2->gpib_ADMR = *(int *)(iVar3 * 2 + 0x4a) + 4;
    tmp2->gpib_AUXMR = *(int *)(iVar3 * 2 + 0x4a) + 5;
    tmp2->gpib_ADR = *(int *)(iVar3 * 2 + 0x4a) + 6;
    iVar6 = *(int *)(iVar3 * 2 + 0x4a) + 7;
  }
```

(original forum post : https://www.eevblog.com/forum/testgear/automating-tektronix-field-adjustment-software-for-tds-oscilloscopes/msg1388658/#msg1388658 )


## How to make a patch for another card

- find the docs for your ISA card and find the addresses of the control registers. See the table above
- is your address map more similar to a PC2 or PC2A ? i.e. a  xxx8, xxx9, xxxA sequence vs 02xx,04xx,06xx ? pick one. I used PC2A; PC2 will require slightly different modifications
- Optional, if based on the PC2A : near file offset 0x6dcd1, modify two occurences of "PC2A" to an arbitrary string that you will use in the config file. I used just "AT".
- Remember, you cannot change the length of the .exe file, and string needs to be 0-terminated.
- tweak the code near file offset 0x346D1 . Any hex editor will do but it helps to look at disassembly to validate.

Then, once the patch is tested to work properly, create a patch file :
- with xdelta3: `xdelta3 -e -0 -f -s orig.exe patched.exe patch_name.xd`
- with pycrk: `crk-generate orig.exe patched.exe -o patch_name.crk`

## How to use one of these patches

They are available either as CRK (text-based) or xdelta files.
- with xdelta3 : `xdelta3 -d -s <path_to_tds700.exe> <patchname.xd> <name_of_output.exe>`
- with pycrk :
Caution, that tool modifies the file in-place. Make a backup before patching. E.g.
```
cp TDS700.exe tds_bak.exe
crk-apply --patch gpib-at.crk
```

## List of patches
#### TDS700.EXE

Original file must be the version with sha256sum `76a0519f70b23c88b26885d140d0bd8e946190d7ea54f4164dfa1954061eea1d`

- gpib-at : support GPIB-AT card with default addresses at 2C0,2C2, etc...

- cec_pc488 : CEC PC-488 (by jozefch), see https://www.eevblog.com/forum/testgear/automating-tektronix-field-adjustment-software-for-tds-oscilloscopes/msg4611313/#msg4611313