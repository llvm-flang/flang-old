! RUN: %flang -verify -fsyntax-only < %s
PROGRAM typetest
  TYPE Point
    REAL X, Y
  END TYPE Point

  type :: pair
    integer(2) i,j
  end type pair

  type person
    character*10 name(2)
    character*20, dimension(5:10) :: save, skills
    integer age
    real, dimension(2,2) :: personality
    integer, save :: id ! expected-error {{use of 'save' attribute specifier in a type construct}}
    real, k :: m ! expected-error {{expected attribute specifier}}
    integer, dimension(3,3) rank ! expected-error {{expected '::'}}
  end type

  type foo 22 ! expected-error {{expected line break or ';' at end of statement}}
    integer :: k
    character(20) :: string
    character(10) str
  end type

  type sphere
    sequence
    real x, y, z, radius
  end type

  type ping
    integer, dimension(11) :: arr1, arr2(13)
    character a(10)*(5), b*8, c(1)
    character*10 :: p*10 ! expected-error {{duplicate character length declaration specifier}}
  end type

  type(Point) p1, p2
  type(sphere) sp

  type( ! expected-error {{expected identifier}}
  type(ping x ! expected-error {{expected ')'}}

  type params ! expected-note {{to match this 'type'}}
    integer i
  i = 22 ! expected-error {{expected 'end type'}}

END PROGRAM typetest
