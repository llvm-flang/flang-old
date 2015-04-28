C RUN: %flang -fsyntax-only %s
C RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
       PROGRAM test
       INTEGERNE,DOI(10),DONE,DOWHILE,DOWHILEI,WHILEI
       DOI=1,10
       ENDDO
C CHECK: done = 1
       DONE=1
C CHECK: done = (1+2)
       DO NE =1+2
C CHECK: do 100 i = 1, 10
       DO100I=1,10
C CHECK: done = 2
100    DON E = 2
C CHECK: do done = 1, 10
       DODONE=1,10
       ENDDO

C CHECK: dowhile = 33
       DOW H ILE=33
C CHECK: dowhilei = 1
       DOWHILEI =1
       DOWHILE(.true.)
       ENDDO

       DOWHILEI=1,10
       ENDDO

C CHECK: doi(1) = 1
       DOI(1) = 1
       DOI(2) = 2

       E ND PRO GRAMt e s t
