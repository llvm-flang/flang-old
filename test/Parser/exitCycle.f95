! RUN: %flang -fsyntax-only < %s
PROGRAM test

  do i = 1,10
    exit
  end do

  outer: do i = 1,10
    inner: do j = 1, 10
      cycle outer
    end do inner
    exit outer
  end do outer

END
