! RUN: %flang -fsyntax-only < %s
PROGRAM vartest
  IMPLICIT NONE
  INTEGER :: I
  REAL :: R
  LOGICAL :: L
  INTEGER I2
  REAL R2
  LOGICAL L2,L3,L4,L5
  DOUBLE PRECISION :: X
  DOUBLE PRECISION X2

  I = 22
  I = -13
  I = +777
  R = 12.0
  L = .TRUE.
  L = .false.

  I2 = I
  R2 = R
  L2 = L

  L3 = L
  L4 = .true.
  L5 = L3

  X = 3.4
  X2 = X

END PROGRAM vartest
