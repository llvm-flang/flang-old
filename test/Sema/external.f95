! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM exttest
  EXTERNAL SUB

  INTEGER FUNC
  REAL FUNC2
  EXTERNAL FUNC, FUNC2

  INTEGER FUNC ! expected-error {{the return type for a function 'func' was already specified}}

  INTEGER F3(10) ! expected-note {{previous definition is here}}
  EXTERNAL F3 ! expected-error {{redefinition of 'f3'}}

  EXTERNAL F4
  COMPLEX F4
  REAL F4 ! expected-error {{the return type for a function 'f4' was already specified}}

  INTEGER I
  REAL R
  COMPLEX C

  I = FUNC() + FUNC2() ! CHECK: i = int((real(func())+func2()))
  C = F4(I) ! CHECK: c = f4(i)

END PROGRAM

subroutine abc1()
   external a
   external a ! expected-error {{duplicate 'external' attribute specifier}}
end

subroutine abc2()
   integer, external :: a
   external a ! expected-error {{duplicate 'external' attribute specifier}}
end

subroutine abc3()
   external a
   integer a
   external a ! expected-error {{duplicate 'external' attribute specifier}}
end

subroutine abc4()
  external a
  integer, external :: a ! expected-error {{duplicate 'external' attribute specifier}}
end

subroutine abc5()
  integer, external :: a
  integer, external :: a ! expected-error {{duplicate 'external' attribute specifier}}
end


