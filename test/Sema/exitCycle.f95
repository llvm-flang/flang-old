! RUN: %flang -fsyntax-only -verify < %s
PROGRAM test

  do i = 1,10
    exit
  end do

  x: do while(.false.)
    cycle x
  end do x

  exit ! expected-error {{'exit' statement not in loop statement}}
  cycle ! expected-error {{'cycle' statement not in loop statement}}

  inner: do i = 1, 10
    cycle outer ! expected-error {{'cycle' statement not in loop statement named 'outer'}}
  end do inner

  outer: if(.false.) then
    exit outer ! expected-error {{'exit' statement not in loop statement named 'outer'}}
  end if outer

END
