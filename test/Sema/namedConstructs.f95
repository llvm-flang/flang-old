! RUN: %flang -verify -fsyntax-only < %s
PROGRAM test

  a: IF(.false.) THEN ! expected-note {{previous definition is here}}
  END IF a

  b: DO I = 1,10
  END DO b

  c: DO WHILE(.false.)
  END DO c

  d: IF(.true.) THEN
  ELSE IF(.false.) THEN d
  ELSE d
  END IF d

  a: IF(.true.) THEN ! expected-error {{redefinition of named construct 'a'}}
  END IF a

  bits: DO I = 1,10 ! expected-note {{to match this 'bits'}}
  END DO ! expected-error {{expected construct name 'bits'}}

  CONTINUE  ! expected-note@+1 {{to match this 'y'}}
  y: IF(.false.) THEN ! expected-note {{to match this 'y'}}
  ELSE IF(.true.) THEN ! expected-error {{expected construct name 'y'}}
  END IF ! expected-error {{expected construct name 'y'}}

  gg: DO WHILE(.false.) ! expected-note {{to match this 'gg'}}
  END DO ha ! expected-error {{expected construct name 'gg'}}

  DO I=1,10
  END DO ha ! expected-error {{use of construct name for an unnamed construct}}

  foo: SELECT CASE(1)
  CASE (1) foo
  CASE DEFAULT foo
  END SELECT foo

  bar: SELECT CASE(1) ! expected-note {{to match this 'bar'}}
  CASE (1) ! expected-error {{expected construct name 'bar'}}
  CASE DEFAULT foo ! expected-error {{expected construct name 'bar'}}
  END SELECT bar ! expected-note@-3 {{to match this 'bar'}}

END
