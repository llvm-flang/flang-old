! RUN: %flang -emit-llvm -o - %s | %file_check %s

PROGRAM helloArrays

  REAL R(10)                ! CHECK: alloca [10 x float]
  INTEGER I(-1:5)           ! CHECK: alloca [7 x i32]
  COMPLEX C(10, 0:9, 10)    ! CHECK: alloca [1000 x { float, float }]
  CHARACTER*20 STR(-5:4, 9) ! CHECK: alloca [90 x [20 x i8]]

  INTEGER ii
  INTEGER (Kind=1) is
  REAL rr
  COMPLEX cc
  CHARACTER*20 STRSTR

  rr = r(1) ! CHECK: getelementptr inbounds [10 x float]*
  CONTINUE  ! CHECK: getelementptr float*
  CONTINUE  ! CHECK: load float*

  ii = i(0) ! CHECK: getelementptr i32*

  is = 1
  i(is) = r(is) ! CHECK: sext i8

  cc = c(1, 1, 1) ! CHECK: getelementptr { float, float }*

  STRSTR = STR(4, 9) ! CHECK: getelementptr [20 x i8]*

  i(5) = 11
  r(1) = 1.0
  c(10, 9, 10) = (1.0, 0.0)
  str(1,1) = 'Hello'

END
