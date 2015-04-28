! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM test
  INTEGER I, J
  CHARACTER (Len = 10) STR, NAME
  LOGICAL L
  I = 0

  SELECT CASE(I)
  CASE (2,3)      ! CHECK:      icmp eq i32
    J = 1         ! CHECK-NEXT: br i1
  CASE (-1:1)     ! CHECK:      icmp sle i32 -1
    J = 0         ! CHECK-NEXT: icmp sle i32 {{.*}}, 1
    continue      ! CHECK-NEXT: and i1
    continue      ! CHECK-NEXT: br i1
  CASE (-10:, :100) ! CHECK:    icmp sle i32 -10
    J = -1        ! CHECK-NEXT: br i1
    continue      ! CHECK:      icmp sle i32 {{.*}}, 100
  CASE DEFAULT    ! CHECK-NEXT: br i1
    J = 42
  END SELECT

  STR = 'Hello World'
  SELECT CASE(STR) ! CHECK:      call i32 @libflang_compare_char1
  CASE ('Hello')   ! CHECK-NEXT: icmp eq i32
    J = 0          ! CHECK-NEXT: br i1
  CASE ('A':'C', 'Foo')
    J = 1
  CASE DEFAULT
    J = 42
  END SELECT

  L = .true.
  SELECT CASE(L)
  CASE (.true.)
    J = 0
  CASE DEFAULT
    J = 42
  END SELECT


END PROGRAM test
