! RUN: %flang -fsyntax-only -verify < %s

program test
  integer x,y,z,w,i,j,k
  real r

  common x,y, /a/ z, /w/ w
  common /a/ i(10,-5:8)
  common // j,k
  common / / r

  common /a/ ! expected-error {{expected identifier}}
  common     ! expected-error {{expected identifier}}

end program

subroutine sub1
  integer i
  common /a/i
  save /a/
end
