! RUN: %flang -fsyntax-only < %s
! NB: this test is for f77 style external arguments, not for f2003 procedure pointers

SUBROUTINE SUB(F, G, H)
  EXTERNAL F
  LOGICAL G
  EXTERNAL G
  EXTERNAL H
  LOGICAL H

  CALL F(2)
  IF(G(3.0)) THEN
  END IF
END

SUBROUTINE F(I)
END

LOGICAL FUNCTION FOO(R)
  Foo = .true.
END

PROGRAM test
  CALL SUB(F, FOO, FOO)
END
