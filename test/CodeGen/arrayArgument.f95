! RUN: %flang -emit-llvm -o - %s | %file_check %s

SUBROUTINE SUB(IARR, IARR2, LEN, RARR)
  INTEGER IARR(10), IARR2(*)
  INTEGER LEN, I, J
  REAL RARR(LEN, *)

  IARR(1) = 11
  IARR2(25) = 13

  RARR(22, 4) = 1.0 ! CHECK: load i32*
  CONTINUE          ! CHECK: mul i64 3
  CONTINUE          ! CHECK: add i64 21
  CONTINUE          ! CHECK: getelementptr float*

  DO I = 1, 10
    IARR(I) = -1 ! CHECK: load i32*
    CONTINUE     ! CHECK: sub i64
    CONTINUE     ! CHECK: getelementptr i32*
  END DO

  DO I = 1, LEN
    DO J = 1, LEN
      RARR(I, J) = 0.0
    END DO
  END DO
END

PROGRAM f77ArrayArgs

  PARAMETER(xdim = 42)
  INTEGER i1(10), i2(50)
  REAL r1(xdim, xdim)

  CALL sub(i1, i2, xdim, r1)

END

