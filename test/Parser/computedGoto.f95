! RUN: %flang -fsyntax-only < %s
PROGRAM gototest
    INTEGER I

10  I = 0
20  GOTO(10) I
    GOTO(10, 20) 3-1
30  I = 3
    GOTO(10, 20, 30) I

END PROGRAM
