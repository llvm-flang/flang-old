! RUN: %flang -fsyntax-only -verify < %s
PROGRAM gototest
    INTEGER I

10  I = 0
    GO TO 10
    GOTO 10

    GO TO HELL ! expected-error {{use of undeclared identifier 'hell'}}
    GO TO 2.0  ! expected-error {{expected statement label after 'GO TO'}}
END PROGRAM
