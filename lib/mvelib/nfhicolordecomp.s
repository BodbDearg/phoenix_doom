;
; Draw a frame of animation into the frame buffer
; I assume all cells are 8*8 pixels in size
;
;void nfHiColorDecomp(Byte *comp,Word x, Word y, Word w, Word h);
;

    AREA    |C$$code|,CODE,READONLY
|x$codeseg|

    EXPORT nfHiColorDecomp
    IMPORT nf_new_w
    IMPORT nf_new_y
    IMPORT nf_new_h
    IMPORT nf_width
    IMPORT nf_new_line
    IMPORT nf_new_row0
    IMPORT nf_back_right
    IMPORT nf_buf_cur
    IMPORT nf_buf_prv
    IMPORT nf_new_x

comp    RN  R0

nfHiColorDecomp
    MOV R12,R13
    STMDB   R13!,{R0-R3}
    STMDB   R13!,{R4-R9,R11,R12,R14,R15}
    SUB R11,R12,#20

    MOV R12,R1
    MOV R1,R2
    MOV R4,R3
    SUB R13,R13,#12
    MOV R5,R12,LSL #4   ;
    LDR R2,Lbnf_new_x
    STR R5,[R2]
    MOV R3,R3,LSL #4
    LDR R2,Lbnf_new_w
    STR R3,[R2]
    MOV R6,R1,LSL #3
    LDR R14,Lbnf_new_y
    STR R6,[R14]
    MOV R14,R6
    LDR R6,[R11,#20]
    MOV R6,R6,LSL #3
    LDR R7,Lbnf_new_h
    STR R6,[R7,#0]
    LDR R6,Lbnf_new_row0
    LDR R6,[R6,#0]
    SUB R3,R6,R3
    STR R3,[R13,#8]
    LDR R6,[R11,#&14]       ;
    MUL R3,R6,R4        ;Get the byte size
    MOV R3,R3,LSL #1        ;Make word count
    LDR R2,Lbnf_buf_cur
    LDR R2,[R2,#0]
    CMP R12,#0
    CMPEQ   R1,#0
    BEQ NoAdd
    LDR R1,Lbnf_width   ;Get the width
    LDR R1,[R1]
    MLA R1,R14,R1,R5        ;Mul by Y + x
    ADD R2,R1,R2        ;Add to the base pointer

NoAdd   MOV R5,R2
    LDR R12,Lbnf_new_line
    LDR R6,[R12]
    MOV R9,R0
    ADD R1,R0,R3
    STR R5,[R13,#4]
    MOV R7,#0   ;Init the height count
ZeroHeightLoop
    MOV R8,#0   ;Init the width count
TryZero LDRB    R3,[R9],#1  ;Get the font data
    LDRB    R12,[R9],#1
    ORRS    R3,R12,R3,LSL #8    ;Zero?
    BNE NotZeroCell
    TST R1,#3
    BNE ZeroByte
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    LDR R3,[R1],#4
    STR R3,[R5],#4
    B   ZeroMerge

ZeroByte
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    ADD R5,R5,R6
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4
    LDRB    R3,[R1],#1
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    LDRB    R12,[R1],#1
    ORR R3,R12,R3,LSL #8
    STR R3,[R5],#4

ZeroMerge
    LDR R3,Lbnf_back_right
    LDR R3,[R3]
    SUB R5,R5,R3
NotZeroCell
    ADD R5,R5,#16   ;Next cell address (Dest screen)
    ADD R8,R8,#1    ;Index to the next record
    CMP R8,R4   ;All cells wide done?
    BCC TryZero
    LDR R3,[R13,#8]
    ADD R5,R5,R3
    ADD R7,R7,#1
    LDR R3,[R11,#20]
    CMP R7,R3
    BCC ZeroHeightLoop

;
; Now process the second pass
;

    MOV R9,R0
    MOV R5,R2
    MOV R0,#0
    STR R5,[R13,#4]
    MOV R7,#0
|L0006ac.J23.nfHiColorDecomp|
    MOV R8,#0
|L0006b4.J25.nfHiColorDecomp|
    LDRB    R0,[R9,#0]      ;Get the font index
    LDRB    R12,[R9,#1]     ;(Little endian!)
    ORRS    R0,R0,R12,LSL #8
    BEQ SkipOffset      ;Skip it?
    TST R0,#&8000
    BEQ PosOffset
    SUB R0,R0,#&C000
    LDR R1,Lbnf_buf_cur
    LDR R1,[R1]
    SUB R1,R5,R1
    ADD R0,R1,R0,LSL #1
    LDR R1,Lbnf_buf_prv
    LDR R1,[R1]
    ADD R1,R1,R0

|L000R1c.J31.nfHiColorDecomp|
           TST  R1,#3
           BNE  OffsetBytes
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    ADD R1,R1,R6
    ADD R5,R5,R6
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    ADD R1,R1,R6
    ADD R5,R5,R6
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    ADD R1,R1,R6
    ADD R5,R5,R6
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    ADD R1,R1,R6
    ADD R5,R5,R6
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    ADD R1,R1,R6
    ADD R5,R5,R6
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    ADD R1,R1,R6
    ADD R5,R5,R6
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    ADD R1,R1,R6
    ADD R5,R5,R6
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    LDR R0,[R1],#4
    STR R0,[R5],#4
    B   OffsetMerge

OffsetBytes
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    ADD R1,R1,R6
    ADD R5,R5,R6
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    ADD     R1,R1,R6
    ADD     R5,R5,R6
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    ADD     R1,R1,R6
    ADD     R5,R5,R6
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    ADD     R1,R1,R6
    ADD     R5,R5,R6
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    ADD     R1,R1,R6
    ADD     R5,R5,R6
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    ADD     R1,R1,R6
    ADD     R5,R5,R6
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    ADD     R1,R1,R6
    ADD     R5,R5,R6
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
    LDRB    R0,[R1],#1
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    LDRB    R12,[R1],#1
    ORR R0,R12,R0,LSL #8
    STR R0,[R5],#4
OffsetMerge
    LDR R0,Lbnf_back_right
    LDR R0,[R0]
    SUB R5,R5,R0
SkipOffset
    ADD R9,R9,#2
    ADD R5,R5,#16
    ADD R8,R8,#1
    CMP R8,R4
    BCC |L0006b4.J25.nfHiColorDecomp|
    LDR R0,[R13,#8]
    ADD R5,R5,R0
    ADD R7,R7,#1
    LDR R0,[R11,#20]
    CMP R7,R0
    BCC |L0006ac.J23.nfHiColorDecomp|
    LDMDB   R11,{R4-R9,R11,R13,R15}

PosOffset   SUB R0,R0,#&4000    ;Convert to direct offset
    ADD R1,R5,R0,LSL #1 ;Word pointer
    B   |L000R1c.J31.nfHiColorDecomp|

Lbnf_width  DCD nf_width
Lbnf_new_line DCD nf_new_line
Lbnf_new_row0 DCD nf_new_row0
Lbnf_back_right DCD nf_back_right
Lbnf_buf_cur DCD    nf_buf_cur
Lbnf_buf_prv DCD    nf_buf_prv
Lbnf_new_x  DCD nf_new_x
Lbnf_new_w  DCD nf_new_w
Lbnf_new_y  DCD nf_new_y
Lbnf_new_h  DCD nf_new_h

    END
