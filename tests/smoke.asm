; Smoke test: Print "HI\n" to MMIO output at $F001, then infinite loop
        LDA #$48      ; 'H'
        STA $F001
        LDA #$49      ; 'I'
        STA $F001
        LDA #$0A      ; '\n'
        STA $F001
loop:
        JMP loop      ; Infinite loop