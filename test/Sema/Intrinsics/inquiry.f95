! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

PROGRAM test
  intrinsic kind, bit_size
  intrinsic selected_int_kind
  intrinsic selected_real_kind

  type point
    integer x,y
  end type

  integer i
  integer(kind=8) i64
  integer i_arr(10)
  integer(kind=1) i8, i8_arr(10)
  integer(kind=bit_size(i8)) i64_arr(10)
  real r
  real (kind=kind(4)) r1
  real (kind=kind(r)) r2
  real (kind=kind(i64_arr)) r3
  real (kind=bit_size(i8_arr)) r4
  complex c
  logical l
  type(point) p

  integer (selected_int_kind(-1)) smallInt
  integer (selected_int_kind(7)) mediumInt
  integer (selected_int_kind(13)) largeInt
  integer (selected_int_kind(1000)) impossible ! expected-error {{invalid kind selector '-1' for type 'integer'}}
  integer (selected_int_kind(i8)) junk ! expected-error {{expected an integer constant expression}}

  parameter (bitsInI32 = bit_size(12))
  parameter (eight = kind(i64))

  parameter (foo = selected_int_kind(3)) ! expected-note@+1 {{this expression is not allowed in a constant expression}}
  parameter (bar = selected_int_kind(i)) ! expected-error {{parameter 'bar' must be initialized by a constant expression}}

  i = kind(i)
  i = kind(i64) + kind(r) * kind(c) ! CHECK: i = (kind(i64)+(kind(r)*kind(c)))
  i = kind(i_arr)
  i = kind(l)
  i = kind(p) ! expected-error {{passing 'type point' to parameter of incompatible type 'intrinsic type'}}

  r = r1 ! CHECK: r = r1
  r = r2 ! CHECK: r = r2
  r = r3 ! CHECK: r = real(r3)
  r4 = r ! CHECK: r4 = real(r,Kind=8)

  i = bit_size(i)       ! CHECK: i = bit_size(i)
  i = bit_size(i64)     ! CHECK: i = int(bit_size(i64))
  i64 = bit_size(i_arr) ! CHECK: i64 = int(bit_size(i_arr),Kind=8)
  i64_arr = i           ! CHECK: i64_arr = int(i,Kind=8)
  i = bit_size(r) ! expected-error {{passing 'real' to parameter of incompatible type 'integer'}}

  i = selected_int_kind(6) ! CHECK: i = selected_int_kind(6)
  i = selected_int_kind(i64)
  i = selected_int_kind(1.0) ! expected-error {{passing 'real' to parameter of incompatible type 'integer'}}
  i = selected_int_kind(i_arr) ! expected-error {{passing 'integer array' to parameter of incompatible type 'integer'}}

  i = selected_real_kind(4) ! CHECK: i = selected_real_kind(4)
  i = selected_real_kind(i64, 20)
  i = selected_real_kind(r) ! expected-error {{passing 'real' to parameter of incompatible type 'integer'}}
  i = selected_real_kind(1, i_arr) ! expected-error {{passing 'integer array' to parameter of incompatible type 'integer'}}

  i = kind() ! expected-error {{too few arguments to intrinsic function call, expected 1, have 0}}
  i = bit_size(i,i) ! expected-error {{too many arguments to intrinsic function call, expected 1, have 2}}

END PROGRAM
