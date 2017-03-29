#!/usr/bin/env zsh

make plans/similar_phrase NOEXEC=1;

thread=46
iter=20
temp=5

# gd=5e-3
gd=1e-3
# gd=2e-3
echo \
  "./bin/plans/similar_phrase \
  V_MODEL_DECOR_FILE_PATH iter${iter}_gd${gd}_temp${temp} \
  V_ITER_NUM ${iter} \
  V_THREAD_NUM ${thread} \
  V_INIT_GRAD_DESCENT_STEP_SIZE ${gd} \
  V_FINAL_INV_TEMP ${temp}"

./bin/plans/similar_phrase \
  V_MODEL_DECOR_FILE_PATH iter${iter}_gd${gd}_temp${temp} \
  V_ITER_NUM ${iter} \
  V_THREAD_NUM ${thread} \
  V_INIT_GRAD_DESCENT_STEP_SIZE ${gd} \
  V_FINAL_INV_TEMP ${temp}
