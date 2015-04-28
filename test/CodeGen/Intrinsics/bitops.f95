! RUN: %flang -emit-llvm -o - %s | %file_check %s

PROGRAM bittest

  integer i
  integer(8) i64
  logical l

  i = 12
  i = not(i) ! CHECK: xor i32 {{.*}}, -1
  i = iand(i, 15) ! CHECK: and i32
  i = iand(1,2)   ! CHECK: store i32 0
  i = ior(i, 1)   ! CHECK: or i32
  i = ior(7,8)    ! CHECK: store i32 15
  i64 = ieor(i64, i64) ! CHECK: xor i64
  i = ieor(1,3)   ! CHECK: store i32 2

  l = btest(i, 2) ! CHECK: and i32 {{.*}}, 4
  continue        ! CHECK-NEXT: icmp ne i32
  continue        ! CHECK-NEXT: select i1 {{.*}}, i1 true, i1 false
  l = btest(8,3)  ! CHECK: store i32 1

  i = ibset(i, 2) ! CHECK: or i32 {{.*}}, 4
  i = ibset(12,1) ! CHECK: store i32 14
  i64 = ibclr(i64, 1) ! CHECK: and i64 {{.*}}, -3
  i = ibclr(14,1) ! CHECK: store i32 12

  i = ibits(i, 2, 4)  ! CHECK: lshr i32 {{.*}}, 2
  continue            ! CHECK-NEXT: and i32 {{.*}}, 15
  i = ibits(14, 1, 3) ! CHECK: store i32 7

  i = ishft(i, 4)  ! CHECK: shl i32 {{.*}}, 4
  i = ishft(i, -2) ! CHECK: lshr i32 {{.*}}, 2
  i = ishft(i, i/4) ! CHECK: icmp sge i32 {{.*}}, 0
  continue          ! CHECK-NEXT: select i1

  i = ishft(3, 1) ! CHECK: store i32 6
  i = ishft(8, -1) ! CHECK: store i32 4

end
