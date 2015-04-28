! RUN: %flang -fsyntax-only -verify < %s

program recovery

  integer i_arr(5)

  do i = 1, 10
    if (i == 0) then ! expected-note {{to match this 'if'}}
  end do ! expected-error {{expected 'end if'}}

  do while(.true.) ! expected-error@+3 {{expected a do termination statement with a statement label '100'}}
    do 100 i = 1, 10   ! expected-note {{to match this 'do'}}
      if (i == 0) then ! expected-note {{to match this 'if'}}
  end do ! expected-error {{expected 'end if'}}

  i = 0

  if(i == 1) then
    do i = 1, 10 ! expected-note {{to match this 'do'}}
  end if ! expected-error {{expected 'end do'}}

  if(i == 1) then
    do i = 1, 10 ! expected-note {{to match this 'do'}}
  else ! expected-error {{expected 'end do'}}
  end if

  if(i == 1) then
    do 100 i = 1,10 ! expected-note {{to match this 'do'}}
  else if(i == 3) then ! expected-error {{expected a do termination statement with a statement label '100'}}
  end if

  do 200 i = 1,10
    do j = 1,10  ! expected-note {{to match this 'do'}}
300   print *,i
200 continue ! expected-error {{expected 'end do'}}

100 continue

    select case(i)
    case (1)
      if (.true.) then ! expected-note {{to match this 'if'}}
    end select ! expected-error {{expected 'end if'}}

    select case(i)
    case (1)
      do while(.false.) ! expected-note {{to match this 'do'}}
    case default ! expected-error {{expected 'end do'}}
    end select

    if(i >= 0) then
      select case(i) ! expected-note {{to match this 'select case'}}
      case (1,2,3)
        i = i + 1
    end if ! expected-error {{expected 'end select'}}

    select case(i)
    case (1)
      do j = 1,10 ! expected-note {{to match this 'do'}}
    case default ! expected-error {{expected 'end do'}}
      end do ! expected-error {{use of 'end do' outside a do construct}}
    end select

    select case(j)
    case (1)
      if(i == 1) then ! expected-note {{to match this 'if'}}
    case default ! expected-error {{expected 'end if'}}
      end if ! expected-error {{use of 'end if' outside an if construct}}
    end select

    select case(i)
    case (1)
      do 400 j = 1, 10 ! expected-note {{to match this 'do'}}
    case (2) ! expected-error {{expected a do termination statement with a statement label '400'}}
400   continue
    end select

    if (i == 1) then
      select case(j) ! expected-note {{to match this 'select case'}}
      case (2)
    end if ! expected-error {{expected 'end select'}}
    end select ! expected-error {{use of 'end select' outside a select case construct}}

    if(i == 1) then
      where(i_arr == 0) ! expected-note {{to match this 'where'}}
    end if ! expected-error {{expected 'end where'}}

end
