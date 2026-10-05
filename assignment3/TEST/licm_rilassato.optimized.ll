; ModuleID = 'licm_rilassato.m2r.ll'
source_filename = "licm_rilassato.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z3funii(i32 noundef %0, i32 noundef %1) #0 {
  %3 = icmp eq i32 %0, 2
  %4 = add nsw i32 %0, 1
  %5 = add nsw i32 %0, 2
  br label %6

6:                                                ; preds = %13, %2
  %.0 = phi i32 [ 0, %2 ], [ %14, %13 ]
  %7 = icmp slt i32 %.0, %0
  br i1 %7, label %8, label %15

8:                                                ; preds = %6
  br i1 %3, label %9, label %10

9:                                                ; preds = %8
  br label %11

10:                                               ; preds = %8
  br label %11

11:                                               ; preds = %10, %9
  %.01 = phi i32 [ %4, %9 ], [ %5, %10 ]
  %12 = add nsw i32 %.01, 1
  br label %13

13:                                               ; preds = %11
  %14 = add nsw i32 %.0, 1
  br label %6, !llvm.loop !6

15:                                               ; preds = %6
  ret i32 0
}

attributes #0 = { mustprogress noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 19.1.7 (6ubuntu3)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
