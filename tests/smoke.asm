; Smoke test: Print "HI\n" to MMIO output at $F001, then exit cleanly
        LDA #$48      ; 'H'
        STA $F001
        LDA #$49      ; 'I'
        STA $F001
        LDA #$0A      ; '\n'
        STA $F001
        LDA #$01      
        STA $F002     ; Triggers bus exit flag -> Computer::run terminates instantly!