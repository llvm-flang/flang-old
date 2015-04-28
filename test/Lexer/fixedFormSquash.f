       PROGRAMfoo
C RUN: %flang -fsyntax-only %s
C RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
       INTEGERI,DOI,IDO
C next line is a DO statement, not DOI =
C CHECK: do i = 1, 10
100    DOI=1,10
       ENDDO
       IDO=I
C CHECK: do doi = 1, 10
       DODOI=1,10
C CHECK: i = ido
        I=I D   O
       ENDDO
C CHECK: assign 100 to i
       ASSIGN100T OI
       IF(I==0)GOTOI
       E ND PRO GRAMfoo
