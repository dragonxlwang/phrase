#!/usr/bin/env zsh

make plans/train NOEXEC=1;

thread=46
iter=20
temp=5

gd=5e-3
./bin/plans/train \
  V_MODEL_DECOR_FILE_PATH iter${iter}_gd${gd}_temp${temp} \
  V_ITER_NUM ${iter} \
  V_THREAD_NUM ${thread} \
  V_INIT_GRAD_DESCENT_STEP_SIZE ${gd} \
  V_FINAL_INV_TEMP ${temp}

gd=2e-3
./bin/plans/train \
  V_MODEL_DECOR_FILE_PATH iter${iter}_gd${gd}_temp${temp} \
  V_ITER_NUM ${iter} \
  V_THREAD_NUM ${thread} \
  V_INIT_GRAD_DESCENT_STEP_SIZE ${gd} \
  V_FINAL_INV_TEMP ${temp}

gd=1e-3
./bin/plans/train \
  V_MODEL_DECOR_FILE_PATH iter${iter}_gd${gd}_temp${temp} \
  V_ITER_NUM ${iter} \
  V_THREAD_NUM ${thread} \
  V_INIT_GRAD_DESCENT_STEP_SIZE ${gd} \
  V_FINAL_INV_TEMP ${temp}
