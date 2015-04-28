! RUN: %flang -interpret %s | %file_check %s

subroutine sub1
  integer i, i_mat(3,3)
  common i, i_mat

  character(10) str
  common /data/ str

  print *,i
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  print *,str

  i = 42
  str = 'WorldHello'
  i_mat(1,:) = -1
end

program com
  integer i, i_mat(3,3)
  common i, i_mat

  character(10) str
  common /data/ str

  i = 2
  i_mat = 1
  str = 'HelloWorld'

  print *, str ! CHECK: HelloWorld

  call sub1 ! CHECK-NEXT: 2
  continue  ! CHECK-NEXT: 1, 1, 1, 1, 1, 1, 1, 1, 1
  continue  ! CHECK-NEXT: HelloWorld

  print *,i ! CHECK-NEXT: 42
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue    ! CHECK-NEXT: -1, 1, 1, -1, 1, 1, -1, 1, 1
  print *,str ! CHECK-NEXT: WorldHello

end


