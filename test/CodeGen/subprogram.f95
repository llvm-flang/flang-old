! RUN: %flang -emit-llvm -o - %s | %file_check %s

SUBROUTINE SUB ! CHECK: define void @sub_()
END            ! CHECK: ret void

SUBROUTINE SUB2(I, R, C, L) ! CHECK: define void @sub2_(i32* noalias %i, float* noalias %r, { float, float }* noalias %c, i32* noalias %l)
  INTEGER I
  REAL R
  COMPLEX C
  LOGICAL L
  INTEGER J

  J = I ! CHECK: load i32* %i
  C = R ! CHECK: load float* %r

  IF(L) THEN ! CHECK: load i32* %l
    J = 0
  END IF

END ! CHECK: ret void

REAL FUNCTION SQUARE(X) ! CHECK: define float @square_(float* noalias %x)
  REAL X                ! CHECK: alloca float
  SQUARE = X * X
  RETURN                ! CHECK: ret float
END

COMPLEX FUNCTION DOUBLE(C)
  COMPLEX C
  DOUBLE = C+C

  CONTINUE ! CHECK: load float*
  CONTINUE ! CHECK: load float*
END

PROGRAM test
  REAL R
  COMPLEX C
  PARAMETER (PI = 3.0)
  INTRINSIC REAL, CMPLX

  REAL ExtFunc
  EXTERNAL ExtSub, ExtSub2, ExtFunc

  R = SQUARE(2.0) ! CHECK: store float 2.0
  CONTINUE        ! CHECK: call float @square_(float*

  R = SQUARE(R)   ! CHECK: call float @square_(float*
  R = SQUARE(SQUARE(R))

  R = SQUARE(PI)  ! CHECK: call float @square_(float*

  C = DOUBLE((1.0, 2.0)) ! CHECK: store float 1
  CONTINUE               ! CHECK: store float 2
  CONTINUE               ! CHECK: call {{.*}} @double_
  C = DOUBLE(DOUBLE(C))

  C = DOUBLE(CMPLX(SQUARE(R)))

  CALL SUB        ! CHECK: call void @sub_

  CALL SUB2(1, 2.0, (1.0, 2.0), .false.)
  CONTINUE ! CHECK: store i32 1
  CONTINUE ! CHECK: store float 2.0
  CONTINUE ! CHECK: store float 1.0
  CONTINUE ! CHECK: store float 2.0
  CONTINUE ! CHECK: store i32 0
  CONTINUE ! CHECK: call void @sub2_

  R = ExtFunc(ExtFunc(1.0)) ! CHECK: call float @extfunc_(float*

  CALL ExtSub      ! CHECK: call void @extsub_()
  CALL ExtSub()    ! CHECK: call void @extsub_()
  CALL ExtSub2(R, C) ! CHECK: call void @extsub2_(float*


END
