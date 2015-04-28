! RUN: %flang -fsyntax-only -verify < %s
PROGRAM gototest
    INTEGER I

10  I = 0
20  GOTO(10, 10) I ! expected-warning {{computed goto statement is deprecated}}

    continue ! expected-warning@+1 {{computed goto statement is deprecated}}
    GOTO (10, 20) .false. ! expected-error {{statement requires an expression of integer type ('logical' invalid)}}

    continue ! expected-warning@+1 {{computed goto statement is deprecated}}
    GOTO (10, 30, 10) 2 ! expected-error {{use of undeclared statement label '30'}}

END PROGRAM
