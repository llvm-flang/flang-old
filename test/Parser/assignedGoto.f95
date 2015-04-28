! RUN: %flang -fsyntax-only -verify < %s
PROGRAM gototest
    INTEGER I
    INTEGER DEST
    REAL R

10  I = 0
20  R = 1.0
    ASSIGN 10 TO DEST

    ASSIGN TO DEST ! expected-error {{expected statement label after 'ASSIGN'}}
    ASSIGN 10 DEST ! expected-error {{expected 'to'}}
    ASSIGN 10 TO 2 ! expected-error {{expected identifier}}

    GO TO DEST
    GOTO DEST (10)
    GO TO DEST (10, 20)

    GO TO DEST (X) ! expected-error {{expected statement label}}
    GOTO DEST (10, 'WRONG') ! expected-error {{expected statement label}}
    GO TO DEST (10 ! expected-error {{expected ')'}}

END PROGRAM
