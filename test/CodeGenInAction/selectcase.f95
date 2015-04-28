! RUN: %flang -interpret %s | %file_check %s
program testselect
  integer i
  logical l

  print *,'start'      ! CHECK: start

  do i = 0,4           ! CHECK-NEXT: default
    select case(i)     ! CHECK-NEXT: right
    case(1,2,3)        ! CHECK-NEXT: right
      print *,'right'  ! CHECK-NEXT: right
    case default       ! CHECK-NEXT: default
      print *,'default'
    end select
  end do

  do i = 0,5           ! CHECK-NEXT: default
    select case(i)     ! CHECK-NEXT: right
    case(1:3, 4)       ! CHECK-NEXT: right
      print *,'right'  ! CHECK-NEXT: right
    case default       ! CHECK-NEXT: right
      print *,'default'! CHECK-NEXT: default
    end select
  end do

  do i = -1,4          ! CHECK-NEXT: right
    select case(i)     ! CHECK-NEXT: right
    case(:1, 3)        ! CHECK-NEXT: right
      print *,'right'  ! CHECK-NEXT: default
    case default       ! CHECK-NEXT: right
      print *,'default'! CHECK-NEXT: default
    end select
  end do

  do i = -1,4          ! CHECK-NEXT: default
    select case(i)     ! CHECK-NEXT: right
    case(0, 2:)        ! CHECK-NEXT: default
      print *,'right'  ! CHECK-NEXT: right
    case default       ! CHECK-NEXT: right
      print *,'default'! CHECK-NEXT: right
    end select
  end do

  do i = 0,1           ! CHECK-NEXT: right
    if(i == 0) then    ! CHECK-NEXT: default
      l = .true.
    else
      l = .false.
    end if

    select case(l)
    case(.true.)
      print *,'right'
    case default
      print *,'default'
    end select
  end do

end
