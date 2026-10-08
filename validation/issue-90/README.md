# PR #90 compatibility validation

Production candidate: `e736970c2c52f6c1d856d1ab42cfbb587a900833`, based on upstream master `35fd1684bf7222ea679013f662697884ff6ebffb`.

The upstream diff contains seven added lines in `volcinstall.sh` and `spec/volclava.spec`. Debian metadata, the release version and the installer helper-path fixes are retained from current master. This validation branch adds the workflow and this record separately; its production files are identical to the candidate.

The workflow runs the real preparation script and full source build in clean Ubuntu 20.04, 22.04 and 24.04 containers. It asserts that 20.04 retains native RPC without either standalone development package, while 22.04/24.04 have both RPC and NIS development packages. On 24.04 it first runs the unmodified master installer and requires configure to reproduce the missing RPC-header failure, then installs only libtirpc-dev and requires the missing-libnsl failure. All successful arms run `bootstrap.sh`, `make -j2` and `make check`, with the selected RPC linkage verified in configure output.

The Rocky 8 arm parses the exact RPM BuildRequires, verifies that EL6/EL7 macro expansion excludes both standalone dependencies and EL8 includes both, installs that set and performs the full build/check. EL6/EL7 checks are metadata expansion on Rocky 8; no native CentOS 6/7 runtime or package build is claimed.

The historically successful [installer matrix](https://github.com/ZedingZhang/volclava/actions/runs/33379779430) verifies the same dependency-selection approach on the older base. It is supporting evidence, not a fresh test of this candidate. Current run results are linked from PR #90 after dispatch.
