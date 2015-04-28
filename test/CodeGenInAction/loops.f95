! RUN: %flang -interpret %s | %file_check %s
PROGRAM loops
  INTEGER I
  INTEGER J

  DO I = 1, 5          ! CHECK: I=1
    PRINT *, 'I=', I   ! CHECK-NEXT: I=2
  END DO               ! CHECK-NEXT: I=3
  CONTINUE             ! CHECK-NEXT: I=4
  CONTINUE             ! CHECK-NEXT: I=5
  PRINT *, 'END'       ! CHECK-NEXT: END

  J = -2
  DO I = 1, J
    PRINT *, 'I=', I
  END DO
  PRINT *, 'END'       ! CHECK-NEXT: END

  DO I = 1, J, -1      ! CHECK-NEXT: I=1
    PRINT *, 'I=', I   ! CHECK-NEXT: I=0
  END DO               ! CHECK-NEXT: I=-1
  CONTINUE             ! CHECK-NEXT: I=-2
  PRINT *, 'END'       ! CHECK-NEXT: END

  DO I = 1,1
    PRINT *, 'I=', I   ! CHECK-NEXT: I=1
  END DO
  PRINT *, 'END'       ! CHECK-NEXT: END

  J = 12
  DO I = 1,J,-1
    PRINT *, 'I=', I
  END DO
  PRINT *, 'END'       ! CHECK-NEXT: END

  I = 0
  DO WHILE(I < 3)      ! CHECK-NEXT: I=0
    PRINT *, 'I=', I   ! CHECK-NEXT: I=1
    I = I + 1
  END DO               ! CHECK-NEXT: I=2
  PRINT *, 'END'       ! CHECK-NEXT: END

  do i = 1,3 ! CHECK-NEXT: I=1
    print *, 'I=', I ! CHECK-NEXT: I=2
    if(i == 2) exit
  end do
  PRINT *, 'END'       ! CHECK-NEXT: END

  i = -1
  loop: do while(.true.)
    i = i + 1
    if(i < 1) cycle loop
    print *, 'I=', I ! CHECK-NEXT: I=1
    if(i >= 2) exit loop ! CHECK-NEXT: I=2
  end do loop
  PRINT *, 'END'       ! CHECK-NEXT: END

  outer: do i = 1,5
    inner: do j = 1,3
      if(i <= 4) cycle outer
      print *, 'I=', J ! CHECK-NEXT: I=1
    end do inner  ! CHECK-NEXT: I=2
  end do outer ! CHECK-NEXT: I=3
  PRINT *, 'END'       ! CHECK-NEXT: END

   do 10 i = 1,10
10 continue
   print *, i ! CHECK-NEXT: 11

   do 20 i = 1,10
   if(i == 5) goto 20
20 continue
   print *, i ! CHECK-NEXT: 11

   do 30 i = 1,10
   if(i == 5) goto 40
30 continue
40 continue
   print *, i ! CHECK-NEXT: 5

END PROGRAM
