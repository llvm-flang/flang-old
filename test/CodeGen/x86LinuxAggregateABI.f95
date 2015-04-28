! RUN: %flang -triple "i686-unknown-linux" -emit-llvm -o - %s | %file_check %s

complex function foo() ! CHECK: define void @foo_({ float, float }*
  foo = (1.0, 2.0)
end

complex(8) function bar() ! CHECK: define void @bar_({ double, double }*
  bar = 0.0
end

program test
  complex c
  complex(8) dc

  c = foo()
  dc = bar()
end
