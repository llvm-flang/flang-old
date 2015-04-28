! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM testcomplexintrinsics
  COMPLEX c
  INTRINSIC aimag, conjg
  REAL r

  c = (1.0, 0.0) ! CHECK: store float
  CONTINUE       ! CHECK: store float
  r = aimag(c)   ! CHECK: store float
  c = conjg(c)   ! CHECK: fsub float
  CONTINUE       ! CHECK: store float
  CONTINUE       ! CHECK: store float

END

