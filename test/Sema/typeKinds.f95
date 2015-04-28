! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

REAL(Kind=8) FUNCTION A()
  A = 1.0 ! CHECK: a = real(1,Kind=8)
END

INTEGER(8) FUNCTION FF(I)
  INTEGER I
  FF = I ! CHECK: ff = int(i,Kind=8)
END

PROGRAM vartest

  REAL(Kind = 4) R
  REAL(Kind = 8) D
  REAL(Kind = 0) A ! expected-error {{invalid kind selector '0' for type 'real'}}

  INTEGER(4) I
  INTEGER(8) J

  COMPLEX(4) C
  COMPLEX(2) Chalf ! expected-error {{invalid kind selector '2' for type 'complex'}}

  LOGICAL(Kind= 2) L2
  LOGICAL(1) L1

! FIXME: add character type kinds.

  R = 0.0
  J = 1
  L2 = .false.

  D = R ! CHECK: d = real(r,Kind=8)
  I = J  ! CHECK: i = int(j,Kind=4)
  L1 = L2 ! CHECK: l1 = logical(l2,Kind=1)

END PROGRAM
