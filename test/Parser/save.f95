! RUN: %flang -verify -fsyntax-only < %s

SUBROUTINE FOO
  INTEGER I
  SAVE
END

SUBROUTINE BAR
  INTEGER I, J
  SAVE I, J
END

SUBROUTINE BAM
  INTEGER K, L
  SAVE K, 11 ! expected-error {{expected identifier}}
END
