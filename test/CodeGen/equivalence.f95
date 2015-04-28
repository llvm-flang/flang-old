! RUN: %flang -emit-llvm -o - %s | %file_check %s

PROGRAM eqtest
  INTEGER I, J ! CHECK: alloca i8, i64 4, align
  REAL X       ! CHECK: getelementptr inbounds i8* {{.*}}, i64 0
  EQUIVALENCE(I, J, X)
  INTEGER IS
  INTEGER I_MAT(4,4), I_MAT2(4,4) ! CHECK: alloca i8, i64 80, align
  EQUIVALENCE (IS, I_MAT) ! CHECK: getelementptr inbounds i8* {{.*}}, i64 16
  EQUIVALENCE (IS, I_MAT2(1,2))

  I = 0
  X = 1.0

  IS = 1
  I_MAT(1,1) = 2
END PROGRAM
