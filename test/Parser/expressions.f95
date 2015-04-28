! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM expressions
  REAL x,y,z,w
  LOGICAL l

  X = 2.0
  y = 1.0
  Z = 2.0
  W = 3.0

  x = x + y-z + w ! CHECK: (((x+y)-z)+w)
  x = x+y * Z ! CHECK: (x+(y*z))
  x = x * y + z ! CHECK: ((x*y)+z)
  x = (x + Y) * z ! CHECK: ((x+y)*z)
  x = x * y ** z ! CHECK: (x*(y**z))
  x = x + y ** z / w ! CHECK: (x+((y**z)/w))
  x = x+y ** (z / w) ! CHECK: (x+(y**(z/w)))

  x = (X + y) * z - w ! CHECK: (((x+y)*z)-w)
  x = x + y * -z ! CHECK: (x+(y*(-z)))

  l = x + y .EQ. z ! CHECK: ((x+y)==z)
  l = x / y .LT. z ! CHECK: ((x/y)<z)
  l = x - y .GT. z ** w ! CHECK: ((x-y)>(z**w))

  x = x
  x = (x)
  x = ( 2 () 3 ! expected-error {{expected ')'}}
  x = (1, ) ! expected-error {{expected an expression}}
  x = (3 ! expected-error {{expected ')'}}

  x = ! expected-error {{expected an expression after '='}}

ENDPROGRAM expressions
