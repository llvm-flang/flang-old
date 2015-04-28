! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM testscalarmath
  INTEGER i
  REAL x
  DOUBLE PRECISION d

  INTRINSIC abs, mod, sign, dim
  INTRINSIC max, min
  INTRINSIC sqrt, exp, log, log10
  INTRINSIC sin, cos, tan
  INTRINSIC asin, acos, atan2
  INTRINSIC sinh, cosh, tanh

  x = 1.0
  i = 7

  i = abs(i)      ! CHECK: select i1
  i = mod(13, i)  ! CHECK: srem i32 13

  x = abs(x)      ! CHECK: call float @llvm.fabs.f32
  x = mod(4.0, x) ! CHECK: frem float 4
  x = sign(x, x)  ! CHECK: select
  x = dim(i, 88)  ! CHECK: icmp sgt i32
  CONTINUE        ! CHECK: sub
  CONTINUE        ! CHECK: select

  x = max(x, 1.0, -7.0) ! CHECK: fcmp oge
  CONTINUE              ! CHECK: select
  CONTINUE              ! CHECK: fcmp oge
  CONTINUE              ! CHECK: select

  x = sqrt(x)     ! CHECK: call float @llvm.sqrt.f32
  x = exp(x)      ! CHECK: call float @llvm.exp.f32
  x = log(x)      ! CHECK: call float @llvm.log.f32
  x = log10(x)    ! CHECK: call float @llvm.log10.f32
  x = sin(x)      ! CHECK: call float @llvm.sin.f32
  x = cos(x)      ! CHECK: call float @llvm.cos.f32
  x = tan(x)      ! CHECK: call float @tanf(float
  x = atan2(2.0,x)! CHECK: call float @atan2f(float 2

  d = sin(d)      ! CHECK: call double @llvm.sin.f64
  d = cosh(d)     ! CHECK: call double @cosh(
  d = exp(d)      ! CHECK: call double @llvm.exp.f64

END
