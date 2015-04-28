! RUN: %flang -fsyntax-only -verify < %s

PROGRAM test
  EXTERNAL SUB
  EXTERNAL FUNC

  CALL SUB
  CALL SUB()
  CALL FUNC(1,2)

  CALL 22 ! expected-error {{expected identifier}}

  CALL FUNC(1,2 ! expected-error {{expected ')'}}
  CALL FUNC 1,2) ! expected-error {{expected '('}}
END
