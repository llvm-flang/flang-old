! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM assignment
  IMPLICIT NONE
  INTEGER i
  REAL  r
  DOUBLE PRECISION d
  COMPLEX c
  LOGICAL l
  CHARACTER * 10 chars

  i = 1
  r = 1.0
  d = 1.0D1
  c = (1.0,2.0)
  l = .false.
  chars = 'STRING'

  i = i ! CHECK: i = i
  i = r ! CHECK: i = int(r)
  i = d ! CHECK: i = int(d)
  i = c ! CHECK: i = int(c)
  i = l ! expected-error{{assigning to 'integer' from incompatible type 'logical'}}
  i = chars ! expected-error{{assigning to 'integer' from incompatible type 'character (Len=10)'}}

  r = i ! CHECK: r = real(i)
  r = r ! CHECK: r = r
  r = d ! CHECK: r = real(d)
  r = c ! CHECK: r = real(c)
  r = l ! expected-error{{assigning to 'real' from incompatible type 'logical'}}
  r = chars ! expected-error{{assigning to 'real' from incompatible type 'character (Len=10)'}}

  d = i ! CHECK: d = real(i,Kind=8)
  d = r ! CHECK: d = real(r,Kind=8)
  d = d ! CHECK: d = d
  d = c ! CHECK: d = real(c,Kind=8)
  d = l ! expected-error{{assigning to 'double precision' from incompatible type 'logical'}}
  d = chars ! expected-error{{assigning to 'double precision' from incompatible type 'character (Len=10)'}}

  c = i ! CHECK: c = cmplx(i)
  c = r ! CHECK: c = cmplx(r)
  c = d ! CHECK: c = cmplx(d)
  c = c ! CHECK: c = c
  c = l ! expected-error{{assigning to 'complex' from incompatible type 'logical'}}
  c = chars ! expected-error{{assigning to 'complex' from incompatible type 'character (Len=10)'}}

  l = l ! CHECK: l = l
  l = i ! expected-error{{assigning to 'logical' from incompatible type 'integer'}}
  l = r ! expected-error{{assigning to 'logical' from incompatible type 'real'}}
  l = d ! expected-error{{assigning to 'logical' from incompatible type 'double precision'}}
  l = c ! expected-error{{assigning to 'logical' from incompatible type 'complex'}}
  l = chars ! expected-error{{assigning to 'logical' from incompatible type 'character (Len=10)'}}

  chars = chars ! CHECK: chars = chars
  chars = i ! expected-error{{assigning to 'character (Len=10)' from incompatible type 'integer'}}
  chars = r ! expected-error{{assigning to 'character (Len=10)' from incompatible type 'real'}}
  chars = d ! expected-error{{assigning to 'character (Len=10)' from incompatible type 'double precision}}
  chars = c ! expected-error{{assigning to 'character (Len=10)' from incompatible type 'complex'}}
  chars = l ! expected-error{{assigning to 'character (Len=10)' from incompatible type 'logical'}}

END PROGRAM assignment
