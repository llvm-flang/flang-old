! RUN: %flang -emit-llvm -o - %s | %file_check %s

program com   ! CHECK: @__BLNK__ = common global { i32, i32, float, { float, float } } zeroinitializer, align 16
  integer i,j ! CHECK: @dir_ = common global { [20 x i8], i32 } zeroinitializer, align 16
  real r
  complex c
  common i,j,r,c

  integer str_len
  character(20) str
  common /dir/ str
  common /dir/ str_len
  save /dir/

  i = 0 ! CHECK: store i32 0, i32* getelementptr inbounds ({ i32, i32, float, { float, float } }* @__BLNK__, i32 0, i32 0)
  r = 1.0
  str_len = j ! CHECK: load i32* getelementptr inbounds ({ i32, i32, float, { float, float } }* @__BLNK__, i32 0, i32 1)
  continue ! CHECK-NEXT: i32* getelementptr inbounds ({ [20 x i8], i32 }* @dir_, i32 0, i32 1)

  str = 'Hello'

end

subroutine sub1
  integer j
  real r
  common j, r
  character(10) str
  common /dir/ str
  j = 1 ! CHECK: store i32 1, i32* getelementptr inbounds ({ i32, float }* bitcast ({ i32, i32, float, { float, float } }* @__BLNK__ to { i32, float }*), i32 0, i32 0)
  r = 1.0
  str = 'Foo'
end
