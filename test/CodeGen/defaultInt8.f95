! RUN: %flang -fdefault-integer-8 -emit-llvm -o - %s | %file_check %s

program test
  integer i ! CHECK: alloca i64
  logical l ! CHECK-NEXT: alloca i64
end
