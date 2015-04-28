! RUN: %flang -fsyntax-only -verify < %s
PROGRAM gototest

10  CONTINUE
    ASSIGN 10 TO I

END PROGRAM

subroutine foo
    implicit none
10  CONTINUE
    ASSIGN 10 TO I ! expected-error {{use of undeclared identifier 'i'}}
end
