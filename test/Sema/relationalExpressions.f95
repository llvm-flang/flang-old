! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM relexpressions
  IMPLICIT NONE
  INTEGER i
  REAL r
  COMPLEX c
  LOGICAL l

  i = 0
  r = 2.0
  c = (1.0,1.0)

  l = i .LT. i ! CHECK: (i<i)
  l = i .EQ. 2 ! CHECK: (i==2)
  l = 3 .NE. i ! CHECK: (3/=i)
  l = i .GT. r ! CHECK: (real(i)>r)
  l = i .LE. r ! CHECK: (real(i)<=r)
  l = i .GE. i ! CHECK: (i>=i)

  l = r .LT. r ! CHECK: (r<r)
  l = r .GT. 2.0 ! CHECK: (r>2)

  l = c .EQ. c ! CHECK: (c==c)
  l = c .NE. c ! CHECK: (c/=c)
  l = c .NE. r ! CHECK: (c/=cmplx(r))
  l = c .LE. c ! expected-error {{invalid operands to a relational binary expression ('complex' and 'complex')}}
  l = c .EQ. 2.0 ! CHECK: (c==cmplx(2))
  l = c .EQ. 2.0d-1

  l = 'HELLO' .EQ. 'WORLD'
  l = 'HELLO' .NE. 'WORLD'

  i = 1 .NE. 2 ! expected-error {{assigning to 'integer' from incompatible type 'logical'}}
  r = 2.0 .LT. 1 ! expected-error {{assigning to 'real' from incompatible type 'logical'}}

  l = l .EQ. l ! expected-error {{invalid operands to a relational binary expression ('logical' and 'logical')}}
  l = .TRUE. .NE. l ! expected-error {{invalid operands to a relational binary expression ('logical' and 'logical')}}
  l = l .LT. .FALSE. ! expected-error {{invalid operands to a relational binary expression ('logical' and 'logical')}}

END PROGRAM
