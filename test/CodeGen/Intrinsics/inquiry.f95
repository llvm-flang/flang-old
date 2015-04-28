! RUN: %flang -emit-llvm -o - %s | %file_check %s

PROGRAM inquirytest

  integer i
  integer(8) i64
  complex(8) c8

  data i / bit_size(23) /

  i = kind(i)   ! CHECK: store i32 4
  i = kind(i64) ! CHECK-NEXT: store i32 8
  i64 = kind(c8) ! CHECK-NEXT: store i64 8

  i = bit_size(i)   ! CHECK-NEXT: store i32 32
  i = bit_size(i64) ! CHECK-NEXT: store i32 64

  i = selected_int_kind(7) ! CHECK-NEXT: store i32 4
  i = selected_int_kind(1000) ! CHECK-NEXT: store i32 -1

  i = selected_int_kind(i)  ! CHECK: call i32 @libflang_selected_int_kind(i32
  i64 = selected_int_kind(i64) ! CHECK: call i32 @libflang_selected_int_kind(i32

end
