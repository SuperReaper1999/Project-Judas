import subprocess,sys,os
# Mount masking affects this child's private namespace only. No source/workspace files change.
subprocess.run(['mount','--bind',sys.argv[1],'/home/conner/Documents/GitHub'],check=True)
assert not os.path.exists('/home/conner/Documents/GitHub/Project-Judas/build/judas')
print('M66 isolation: isolated network namespace; repository/consumer sources masked; read-only moved package',flush=True)
os.execv(sys.argv[2],[sys.argv[2]])
