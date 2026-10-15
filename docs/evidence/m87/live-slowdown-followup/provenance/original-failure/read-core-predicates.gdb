set debuginfod enabled off
set pagination off
info registers fs_base
x/7ug $fs_base-96
thread apply all bt 1
quit
