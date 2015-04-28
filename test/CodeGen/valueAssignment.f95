! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM test
  INTEGER X
  REAL Y
  LOGICAL L
  COMPLEX C

  PARAMETER(IC = 0)
  PARAMETER(RC = 1.2)
  PARAMETER(CC = (1.0, 2.0))
  PARAMETER(LC = .true. .EQV. .false.)

  X = 1          ! CHECK: store i32 1, i32*
  Y = 0.0        ! CHECK: store float 0
  L = .false.    ! CHECK: store i32 0, i32*

  CONTINUE       ! CHECK: getelementptr inbounds { float, float }*
  C = (1.0, 3.0) ! CHECK: store float 1
  CONTINUE       ! CHECK: getelementptr inbounds { float, float }*
  CONTINUE       ! CHECK: store float 3

  X = IC
  Y = RC
  C = CC
  L = LC

END
