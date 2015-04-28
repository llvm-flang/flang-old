! RUN: %flang -fsyntax-only -verify < %s

SUBROUTINE FOO()
  RETURN
END

FUNCTION BAR()
  RETURN
END

PROGRAM main
  RETURN ! expected-error {{'RETURN' statement should be used inside a function or a subroutine}}
END
