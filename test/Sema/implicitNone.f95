! RUN: %flang -fsyntax-only -verify < %s
PROGRAM imptest
  IMPLICIT NONE
  IMPLICIT REAL (Y) ! expected-error {{use of 'IMPLICIT' after 'IMPLICIT NONE'}}
  INTEGER X

  X = 1
  Y = X ! expected-error {{use of undeclared identifier 'y'}}
  I = 0 ! expected-error {{use of undeclared identifier 'i'}}

END PROGRAM imptest
