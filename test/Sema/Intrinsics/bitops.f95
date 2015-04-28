! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

PROGRAM test
  intrinsic iand, ieor, ior
  intrinsic not
  intrinsic btest, ibclr, ibset, ibits, ishft, ishftc

  integer i, i_arr(10), i_mat(4,4)
  integer(8) i64, i64_arr(10)
  logical l, l_arr(10)

  i = not(i)
  i = not(2.0) ! expected-error {{passing 'real' to parameter of incompatible type 'integer'}}
  i = not(i64) ! CHECK: i = int(not(i64))

  i = iand(i,1)
  i = ior(i,i) ! CHECK: i = ior(i, i)
  i = ieor(2,i)
  i = iand(i64,i) ! expected-error {{conflicting types in arguments 'i' and 'j' ('integer (Kind=8)' and 'integer')}}
  i = iand(i,2.0) ! expected-error {{conflicting types in arguments 'i' and 'j' ('integer' and 'real')}}
  i_arr = iand(i_arr,7)
  i64 = ior(i64,int(4,8))
  i = ieor(2.0,2.0) ! expected-error {{passing 'real' to parameter of incompatible type 'integer'}}

  l = btest(1,2)
  i = ibclr(i,2)
  i = ibset(i,2)
  i = ibits(i,2,2)
  l = btest(i64,2)
  i64 = ibset(i64,2)
  i64 = ibclr(i64,i)
  i = ibset(3,2.0) ! expected-error {{passing 'real' to parameter of incompatible type 'integer'}}
  l_arr = btest(i_arr, i_mat) ! expected-error {{conflicting shapes in arguments 'i' and 'pos' (1 dimension and 2 dimensions)}}

  i = ishft(i,-2)
  i = ishft(i,4)
  i = ishftc(i,i)

end
