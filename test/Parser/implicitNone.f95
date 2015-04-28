! RUN: %flang -fsyntax-only -verify < %s
PROGRAM imptest
  IMPLICIT NONE (V) ! expected-error {{expected line break or ';' at end of statement}}
END
