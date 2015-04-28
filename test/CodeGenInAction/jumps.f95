! RUN: %flang -interpret %s | %file_check %s
PROGRAM jumps
  INTEGER I
  INTEGER J

  DATA I/0/

1 PRINT *, 'first'  ! CHECK: first
  I = I + 1         ! CHECK-NEXT: first
  IF(I < 2) GOTO 1
  PRINT *, 'end'    ! CHECK-NEXT: end

  I = 1
  GOTO 2
3 PRINT *, 'not'
  I = 0
2 PRINT *, 'yes'    ! CHECK-NEXT: yes

  ASSIGN 3 TO J     ! CHECK-NEXT: not
  IF(I == 1) GO TO J! CHECK-NEXT: yes

  GOTO(1,2,1) 0
  PRINT *, 'twin'   ! CHECK-NEXT: twin

  I = 0
  GOTO(4) 1
5 I = 1
  PRINT *, 'win'    ! CHECK-NEXT: win
4 IF(I == 0) GOTO(4,5) 2
  PRINT *, 'done'   ! CHECK-NEXT: done

END PROGRAM
