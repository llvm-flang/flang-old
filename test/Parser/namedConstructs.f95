! RUN: %flang -fsyntax-only < %s
PROGRAM test

  a: IF(.false.) THEN
  END IF a

  b: DO I = 1,10
  END DO b

1 c: DO WHILE(.false.)
  END DO c

  d: IF(.true.) THEN
  ELSE IF(.false.) THEN d
  ELSE d
  END IF d

END
