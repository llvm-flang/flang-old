! RUN: %flang -interpret %s | %file_check %s

subroutine sub(len, i_mat)
  integer len, i_mat(len, *)

  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
end

program test
  integer i_mat(3,3), i_mat2(3,3)

  print *, 'START' ! CHECK: START
  i_mat = 1
  i_mat(1,1) = 0
  i_mat(2,2) = 0
  i_mat(3,3) = 0
  call sub(3, -i_mat + 1) ! CHECK-NEXT: 1, 0, 0, 0, 1, 0, 0, 0, 1
end
