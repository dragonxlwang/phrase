#!/usr/bin/env zsh

make plans/train NOEXEC=1;

iter=20

gd=1e-2
./bin/plans/train \
  V_MODEL_DECOR_FILE_PATH iter${iter}_gd${gd} \
  V_ITER_NUM ${iter} \
  V_THREAD_NUM 46 \
  V_INIT_GRAD_DESCENT_STEP_SIZE ${gd}

gd=5e-3
./bin/plans/train \
  V_MODEL_DECOR_FILE_PATH iter${iter}_gd${gd} \
  V_ITER_NUM ${iter} \
  V_THREAD_NUM 46 \
  V_INIT_GRAD_DESCENT_STEP_SIZE ${gd}

gd=1e-3
./bin/plans/train \
  V_MODEL_DECOR_FILE_PATH iter${iter}_gd${gd} \
  V_ITER_NUM ${iter} \
  V_THREAD_NUM 46 \
  V_INIT_GRAD_DESCENT_STEP_SIZE ${gd}

