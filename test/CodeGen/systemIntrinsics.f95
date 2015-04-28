! RUN: %flang -emit-llvm -o - %s | %file_check %s

PROGRAM sys ! CHECK: call void @libflang_sys_init()

  REAL ETIME
  INTRINSIC ETIME
  REAL R_ARR(2)
  REAL T

  T = ETIME(R_ARR) ! CHECK: call float @libflang_etimef(float* {{.*}}, float*

END
