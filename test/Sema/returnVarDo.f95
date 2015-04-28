! RUN: %flang -fsyntax-only -verify < %s

INTEGER FUNCTION I()
  DO I = 1,10
  END DO
  DO I = 1,10 ! expected-note {{which is used in a do statement here}}
    I = 1 ! expected-error {{assignment to a do variable 'i'}}
  END DO
END

FUNCTION J()
  DO J = 1,10
  END DO
END
