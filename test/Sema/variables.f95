! RUN: %flang -fsyntax-only -verify < %s
PROGRAM vartest
  PARAMETER (Y = 2.0) ! expected-note {{previous definition is here}}

  PARAMETER (PI = 3.14) ! expected-note {{previous definition is here}}
  PARAMETER (PI = 4.0) ! expected-error {{redefinition of 'pi'}}

  INTEGER :: I ! expected-note {{previous definition is here}}
  INTEGER :: I ! expected-error {{redefinition of 'i'}}

  INTEGER :: X ! expected-note {{previous definition is here}}
  REAL :: X ! expected-error {{redefinition of 'x'}}

  REAL :: Y ! expected-error {{redefinition of 'y'}}

  i = K

END PROGRAM
