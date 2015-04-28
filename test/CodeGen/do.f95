! RUN: %flang -emit-llvm -o - %s
PROGRAM dowhiletest
  INTEGER I
  INTEGER J
  REAL R, Z

  J = 1
  DO I = 1, 10
    J = J * I
  END DO

  DO I = 1, 10, -2
    J = J - I
  END DO

  Z = 0.0
  DO R = 1.0, 2.5, 0.25
    Z = Z + R
  END DO

END PROGRAM
