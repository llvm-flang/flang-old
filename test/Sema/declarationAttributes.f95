! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

program declAttrTest

  implicit none

  real, external :: sub ! CHECK: real subroutine sub()
  logical, intrinsic :: lle

  integer, dimension(10,10) :: i_mat
  real, dimension(10) :: m(20), k

  integer, external, external :: foo ! expected-error {{duplicate 'external' attribute specifier}}
  integer, dimension(20), &
           dimension(40) :: vector ! expected-error {{duplicate 'dimension' attribute specifier}}

  if(lle('a','b')) then
  end if

  ! FIXME: support other attributes.

end
