! RUN: %flang -fsyntax-only -verify < %s
PROGRAM charexpressions
  IMPLICIT NONE
  CHARACTER * 16 C

  C = 'HELLO' // 'WORLD'
  C = C // '!'

  C = C // 1 ! expected-error {{invalid operands to a character binary expression ('character (Len=16)' and 'integer')}}
  C = .false. // 'TRUE' ! expected-error {{invalid operands to a character binary expression ('logical' and 'character')}}

ENDPROGRAM charexpressions
