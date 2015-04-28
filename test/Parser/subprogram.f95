! RUN: %flang -fsyntax-only -verify < %s

SUBROUTINE FOO
END

SUBROUTINE BAR(X,Y)
  INTEGER X,Y
END SUBROUTINE

FUNCTION F()
END

REAL FUNCTION FF()
END

FUNCTION F2(X,Y)
  REAL X,Y
  X = FF( ! expected-error {{expected ')'}}
END FUNCTION

SUBROUTINE SUB( ! expected-error {{expected ')'}}

END

FUNCTION FUNC ! expected-error {{expected '('}}
ENDFUNCTION


