! RUN: %flang -fdefault-real-8 -fdefault-double-8 -emit-llvm -o - %s | %file_check %s

program test
  real x             ! CHECK:      alloca double
  double precision y ! CHECK-NEXT: alloca double
  integer i          ! CHECK-NEXT: alloca i32
end
