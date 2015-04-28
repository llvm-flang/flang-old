! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM test
  INTEGER X ! CHECK: alloca i32
  REAL Y    ! CHECK: alloca float
  LOGICAL L ! CHECK: alloca i32
  DOUBLE PRECISION DP ! CHECK: alloca double

  X = X     ! CHECK: load i32*
  X = +X    ! CHECK: load i32*
  X = -X    ! CHECK: sub i32 0

  X = X + X ! CHECK: add i32
  X = X - X ! CHECK: sub i32
  X = X * X ! CHECK: mul i32
  X = X / X ! CHECK: sdiv i32

  Y = Y     ! CHECK: load float*
  Y = -Y    ! CHECK: fsub

  Y = Y + Y ! CHECK: fadd float
  Y = Y - Y ! CHECK: fsub float
  Y = Y * Y ! CHECK: fmul float
  Y = Y / Y ! CHECK: fdiv float

  X = 1 + X   ! CHECK: add i32 1
  Y = 2.5 * Y ! CHECK: fmul float 2.5

  L = 1 .EQ. X ! CHECK: icmp eq i32 1
  L = 0 .NE. X ! CHECK: icmp ne i32 0
  L = 42 .LE. X ! CHECK: icmp sle i32 42
  L = X .LT. X ! CHECK: icmp slt i32
  L = X .GE. X ! CHECK: icmp sge i32
  L = X .GT. X ! CHECK: icmp sgt i32

  L = 1.0 .EQ. Y ! CHECK: fcmp oeq float 1
  L = 0.0 .NE. Y ! CHECK: fcmp une float 0
  L = Y .LE. Y   ! CHECK: fcmp ole float
  L = Y .LT. Y   ! CHECK: fcmp olt float
  L = Y .GE. Y   ! CHECK: fcmp oge float
  L = Y .GT. Y   ! CHECK: fcmp ogt float

  Y = Y ** 4.0   ! CHECK: call float @llvm.pow.f32.f32
  Y = Y ** 5     ! CHECK: call float @llvm.powi.f32.i32
  X = X ** 2     ! CHECK: call i32 @libflang_pow_i4_i4

  DP = DP         ! CHECK: load double*
  DP = 1.0d0 + DP ! CHECK: fadd double 1
  L  = DP .EQ. DP ! CHECK: fcmp oeq double
  DP = DP ** 2    ! CHECK: call double @llvm.powi.f64.i32

END PROGRAM
