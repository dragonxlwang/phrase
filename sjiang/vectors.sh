#!/usr/bin/env bash

function check_cmd_arg  {
  argarr=($(echo $1))
  xarg=$2
  for x in "${argarr[@]}"; do
    if [[ $x == $xarg ]]; then
      echo 1
      return 0
    fi
  done
  echo 0
  return 1
}

train_debug_opt=$(check_cmd_arg "$*" DEBUG)
train_peek_opt=$(check_cmd_arg "$*" PEEK)
CFLAGS=""
[[ $train_debug_opt -eq 1  ]] && { CFLAGS="-DDEBUG "; }
[[ $train_peek_opt -eq 1  ]] && { CFLAGS+="-DDEBUGPEEK "; }

nomake=$(check_cmd_arg "$*" nomake)

if [[ $nomake -eq 0  || $1 == "make" ]]; then
  make vectors/train NOEXEC=1 CFLAGS="$CFLAGS"
  make eval/eval_peek NOEXEC=1
  make eval/eval_word_distance NOEXEC=1
  make eval/eval_question_accuracy NOEXEC=1 # CFLAGS="-DACCURACY"
  if [[ $1 == "make" ]]; then exit 0; fi
fi

train="./bin/vectors/train"
peek="./bin/eval/eval_peek"
wd="./bin/eval/eval_word_distance"
qa="./bin/eval/eval_question_accuracy"
if [[ $1 == "train" ]]; then bin=$train
elif [[ $1 == "peek" ]]; then bin=$peek
elif [[ $1 == "wd" ]]; then bin=$wd
elif [[ $1 == "qa" ]]; then bin=$qa
else bin=$train
fi

eval "model=\$$2"
$bin $model
