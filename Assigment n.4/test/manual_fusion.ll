; IR manuale molto semplice per testare il pass loop-fusion-pass.
; Due loop single-block, adiacenti, stesso trip count.
; Il pass sposta il corpo utile del secondo loop nel primo loop.

; Esecuzione suggerita:
; opt -S -load-pass-plugin ./LoopFusionPass.so -passes="loop-fusion-pass" ../test/manual_fusion.ll -o manual_fused.ll

define void @fusion_manual(ptr %A, ptr %B, ptr %C, i32 %n) {
entry:
  br label %loop1

loop1:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop1 ]
  %idx1 = sext i32 %i to i64
  %b.ptr = getelementptr inbounds i32, ptr %B, i64 %idx1
  %b.val = load i32, ptr %b.ptr, align 4
  %a.val = add i32 %b.val, 1
  %a.ptr = getelementptr inbounds i32, ptr %A, i64 %idx1
  store i32 %a.val, ptr %a.ptr, align 4
  %i.next = add nuw nsw i32 %i, 1
  %cmp1 = icmp slt i32 %i.next, %n
  br i1 %cmp1, label %loop1, label %between

between:
  br label %loop2

loop2:
  %j = phi i32 [ 0, %between ], [ %j.next, %loop2 ]
  %idx2 = sext i32 %j to i64
  %a2.ptr = getelementptr inbounds i32, ptr %A, i64 %idx2
  %a2.val = load i32, ptr %a2.ptr, align 4
  %c.val = add i32 %a2.val, 2
  %c.ptr = getelementptr inbounds i32, ptr %C, i64 %idx2
  store i32 %c.val, ptr %c.ptr, align 4
  %j.next = add nuw nsw i32 %j, 1
  %cmp2 = icmp slt i32 %j.next, %n
  br i1 %cmp2, label %loop2, label %exit

exit:
  ret void
}
