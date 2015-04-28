! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM test
  INTEGER I
  REAL X
  COMPLEX C
  LOGICAL L
  DOUBLE PRECISION D
  DOUBLE COMPLEX CD

  I = 0
  X = 1.0
  C = (1.0,0.0)
  D = 1.0d1

  X = X + I ! CHECK: sitofp i32
  CONTINUE  ! CHECK: fadd float
  CONTINUE  ! CHECK: store float

  I = X - I ! CHECK: sitofp i32
  CONTINUE  ! CHECK: fsub float
  CONTINUE  ! CHECK: fptosi float
  CONTINUE  ! CHECK: store i32

  L = X .EQ. I ! CHECK: sitofp i32
  CONTINUE     ! CHECK: fcmp oeq float

  X = C    ! CHECK: store float
  I = C    ! CHECK: fptosi float
  CONTINUE ! CHECK: store i32
  C = X    ! CHECK: store float
  CONTINUE ! CHECK: store float 0
  C = I    ! CHECK: sitofp i32
  CONTINUE ! CHECK: store float
  CONTINUE ! CHECK: store float 0

  C = C + X
  C = C - I

  D = X     ! CHECK: fpext float
  X = D     ! CHECK: fptrunc double
  D = D + X ! CHECK: fadd double
  I = D     ! CHECK: fptosi double
  D = I     ! CHECK: sitofp i32

  C = D     ! CHECK: fptrunc
  CONTINUE  ! CHECK: store float 0

  CD = C    ! CHECK: fpext float
  CONTINUE  ! CHECK: fpext float

  CD = I    ! CHECK: sitofp i32
  CONTINUE  ! CHECK: store double 0

END
