#!/usr/bin/env bash
set -euo pipefail

mkdir -p validation/results
git show 35fd1684bf7222ea679013f662697884ff6ebffb:lsbatch/daemons/mbd.serv.c >validation/baseline-mbd.serv.c

compile() {
  local version="$1"
  local source="$2"
  cc -g -O0 -ffunction-sections -fdata-sections \
    -DHAVE_CONFIG_H -DMBD_SERV_SOURCE="\"$source\"" \
    -I. -Ilsf -Ilsf/lib -Ilsbatch -Ilsbatch/lib -Ilsbatch/daemons \
    -I/usr/include/tirpc validation/submit-hosts.c \
    -Wl,--gc-sections -Wl,--start-group \
    lsbatch/lib/liblsbatch.a lsf/lib/liblsf.a lsf/intlib/liblsfint.a \
    -Wl,--end-group -ltirpc -lnsl -ltcl -lm -pthread \
    -o "validation/results/$version"
}

compile baseline baseline-mbd.serv.c
compile fixed ../lsbatch/daemons/mbd.serv.c

set +e
valgrind --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  --log-file=validation/results/baseline-valgrind.log validation/results/baseline
baseline_status=$?
set -e
cat validation/results/baseline-valgrind.log
test "$baseline_status" -eq 99
grep -Eq 'definitely lost: [1-9][0-9,]* bytes' validation/results/baseline-valgrind.log

valgrind --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  --log-file=validation/results/fixed-valgrind.log validation/results/fixed
cat validation/results/fixed-valgrind.log

{
  echo 'Baseline reproduces the issue; the fixed request lifecycle passes.'
  echo 'Both binaries execute production initSubmit() and real submit/modify XDR codecs.'
  echo 'Each binary processes 1000 submit and 1000 modify requests plus late/early decode failures.'
} >>"$GITHUB_STEP_SUMMARY"
