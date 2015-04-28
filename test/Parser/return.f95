! RUN: %flang -fsyntax-only < %s

SUBROUTINE FOO
  RETURN
END

FUNCTION BAR()
  BAR = 1.0
  RETURN
END
