! RUN: %flang -emit-llvm -o - -O1 %s | %file_check %s
PROGRAM testcomplexintrinsicmath
  COMPLEX c
  INTRINSIC abs, sqrt, sin, cos, log, exp

  c = (1.0, 0.0)

  c = abs(c)   ! CHECK: call float @libflang_cabsf
  c = sqrt(c)  ! CHECK: call void @libflang_csqrtf(float {{.*}}, float {{.*}}, { float, float }*
  c = sin(c)   ! CHECK: call void @libflang_csinf(float {{.*}}, float {{.*}}, { float, float }*
  c = cos(c)   ! CHECK: call void @libflang_ccosf(float {{.*}}, float {{.*}}, { float, float }*
  c = log(c)   ! CHECK: call void @libflang_clogf(float {{.*}}, float {{.*}}, { float, float }*
  c = exp(c)   ! CHECK: call void @libflang_cexpf(float {{.*}}, float {{.*}}, { float, float }*

END
