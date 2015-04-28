! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM testscalartruncround
  INTEGER i
  REAL x

  INTRINSIC aint, anint, nint, ceiling, floor

  x = 2.25

  x = aint(x)  ! CHECK: call float @llvm.trunc.f32
  x = -1.75
  x = anint(x) ! CHECK: call float @llvm.rint.f32
  x = 3.75
  i = nint(x)  ! CHECK: call float @llvm.rint.f32
  CONTINUE     ! CHECK: fptosi
  CONTINUE     ! CHECK: store i32

  i = ceiling(x) ! CHECK: call float @llvm.ceil.f32
  i = floor(x)   ! CHECK: call float @llvm.floor.f32

END PROGRAM
