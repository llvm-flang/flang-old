! RUN: %flang -fsyntax-only -verify < %s
PROGRAM charsubstring
  CHARACTER (LEN=16) :: C
  CHARACTER * 10 C_ARR
  DIMENSION C_ARR(4)

  C = 'HELLO'
  C = 'HELLO'(1:3)
  C = 'HELLO'(1:4 ! expected-error {{expected ')'}}
  C = 'HELLO'(1 4) ! expected-error {{expected ':'}}
  C = 'HELLO'(:)
  C = 'HELLO'(2:)
  C = 'HELLO'(:3)

  C = C(1:2)
  C = C(:)
  C = (C(2:))
  C = C(1 8) ! expected-error {{expected ':'}}
  C = C(:(5+2) ! expected-error {{expected ')'}}
  C = C(: ::  ! expected-error {{expected an expression}}
  continue      ! expected-error@-1 {{expected ')'}}
  C = C( :: : ) ! expected-error {{expected an expression}}

  C_ARR(1) = C
  C_ARR(2) = C_ARR(1)(1:)
  C_ARR(3)(1:3) = (C_ARR(2)(:))

ENDPROGRAM charsubstring
