! RUN: %flang -fsyntax-only -verify < %s

RECURSIVE SUBROUTINE SUB()
END

RECURSIVE FUNCTION FOO()
END

RECURSIVE INTEGER FUNCTION FUNC()
  FUNC = 1
END

INTEGER RECURSIVE FUNCTION FUNC2()
  FUNC2 = 2
END

RECURSIVE INTEGER RECURSIVE FUNCTION M() ! expected-error {{expected 'function'}}
END

RECURSIVE PROGRAM main ! expected-error {{expected 'function' or 'subroutine' after 'recursive'}}
END
