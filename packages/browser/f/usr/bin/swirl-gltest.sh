#!/usr/bin/dash
# SwirlOS GPU 3D test, stage 2 (started by the Terminal command gltest)
export HOME=/home/user EGL_LOG_LEVEL=info MESA_DEBUG=1 LIBGL_DEBUG=verbose
/usr/bin/gltest > /system/debug/gltest-log.txt 2>&1
echo "(exit code $?)" >> /system/debug/gltest-log.txt
