! RUN: %flang -emit-llvm -o - %s | %file_check %s

SUBROUTINE SUB() ! CHECK: @sub_i_ = {{.*}} global i32
  INTEGER I      ! CHECK: @foo_mat_ = {{.*}} global [16 x float]
  SAVE I         ! CHECK: @func_arr_ = {{.*}} global [10 x i32]

  I = 0 ! CHECK: store i32 0, i32* @sub_i_
END

SUBROUTINE FOO(I)
  INTEGER I
  REAL MAT(4,4)
  SAVE

  IF(I == 0) MAT(1,1) = 42.0 ! CHECK: store float {{.*}}[16 x float]* @foo_mat_
END

FUNCTION FUNC(I)
  INTEGER I, K, FUNC
  INTEGER ARR(10)
  SAVE ARR
  DATA ARR / 10*0 /
  DATA K / 1 /

  ARR(I+1) = 42
  FUNC = ARR(I)
  CALL FOO(I+1)
END
