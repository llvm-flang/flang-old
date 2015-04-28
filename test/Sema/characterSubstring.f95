! RUN: %flang -fsyntax-only -verify < %s
PROGRAM charsubstring
  CHARACTER (LEN=16) :: C

  C = 'HELLO'(1:3)
  C = 'HELLO'(1:'FALSE') ! expected-error {{expected an expression of integer type ('character' invalid)}}
  C = 'HELLO'(1:)
  C = 'HELLO'(:) ! expected-error@+1 {{expected an expression of integer type ('real' invalid)}}
  C = C( 4.0: 3.0) ! expected-error {{expected an expression of integer type ('real' invalid)}}

ENDPROGRAM charsubstring
