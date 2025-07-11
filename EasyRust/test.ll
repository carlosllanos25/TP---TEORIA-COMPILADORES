; ModuleID = 'EasyRustModule'
source_filename = "EasyRustModule"

@fmt = private unnamed_addr constant [5 x i8] c"%lf\0A\00", align 1

declare i32 @printf(ptr, ...)

declare double @exp(double)

define i32 @main() {
entry:
  %contador = alloca i32, align 4
  store i32 0, ptr %contador, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %contador1 = load i32, ptr %contador, align 4
  %cmp = icmp slt i32 %contador1, 5
  br i1 %cmp, label %while.body, label %while.exit

while.body:                                       ; preds = %while.cond
  %contador2 = load i32, ptr %contador, align 4
  %int_to_double = sitofp i32 %contador2 to double
  %printf_call = call i32 (ptr, ...) @printf(ptr @fmt, double %int_to_double)
  %contador3 = load i32, ptr %contador, align 4
  %addtmp = add i32 %contador3, 1
  store i32 %addtmp, ptr %contador, align 4
  br label %while.cond

while.exit:                                       ; preds = %while.cond
  ret i32 0
}
