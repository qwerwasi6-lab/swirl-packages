#!/usr/bin/dash
export GIO_MODULE_DIR=/usr/lib/x86_64-linux-gnu/gio/modules SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt LIBGL_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri GBM_BACKENDS_PATH=/usr/lib/x86_64-linux-gnu/gbm LP_NUM_THREADS=0 JSC_useJIT=false JSC_useConcurrentJIT=false DISPLAY=:0 HOME=/home/user WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1 GDK_BACKEND=x11 NO_AT_BRIDGE=1 GSETTINGS_BACKEND=memory JSC_usePollingTraps=true
# r127: draw web pages on the GPU (Mesa crocus through /dev/dri) when SwirlOS offers one.
# Touch /home/user/.browser-software to go back to the old software renderer.
if [ -e /dev/dri/renderD128 ] && [ ! -e /home/user/.browser-software ]; then
  export WEBKIT_FORCE_COMPOSITING_MODE=1 WEBKIT_FORCE_DMABUF_RENDERER=1 WEBKIT_DMABUF_RENDERER_FORCE_SHM=1 WEBKIT_WEB_RENDER_DEVICE_FILE=/dev/dri/renderD128
  echo "renderer: GPU" > /home/user/browser-renderer.txt
else
  export MESA_LOADER_DRIVER_OVERRIDE=swrast LIBGL_ALWAYS_SOFTWARE=1 WEBKIT_DISABLE_DMABUF_RENDERER=1 WEBKIT_DISABLE_COMPOSITING_MODE=1
  echo "renderer: software" > /home/user/browser-renderer.txt
fi
printf 'multi on\n' > /etc/host.conf
printf 'passwd: files\ngroup: files\nhosts: files dns\n' > /etc/nsswitch.conf
try=0
while [ $try -lt 4 ]; do
  /usr/bin/Xvfb :0 +iglx -extension MIT-SHM -screen 0 800x560x24 -nolisten tcp -noreset > /home/user/browser-xvfb-log.txt 2>&1 &
  XPID=$!
  n=0
  while [ $n -lt 40 ]; do
    if /usr/bin/xdpyinfo > /dev/null 2>&1; then break 2; fi
    kill -0 $XPID 2> /dev/null || break
    /usr/bin/sleep 1; n=$((n+1))
  done
  try=$((try+1))
done
( while :; do /usr/bin/xbridge >> /tmp/xb.log 2>&1; /usr/bin/sleep 2; done ) &
/usr/bin/sleep 1
# r123: if the browser itself dies, keep its log and start it again (5 tries)
n=0
while [ $n -lt 5 ]; do
  /usr/lib/x86_64-linux-gnu/webkit2gtk-4.1/MiniBrowser "${1:-file:///usr/share/swirl-home.html}" > /home/user/browser-log.txt 2>&1
  /usr/bin/cat /home/user/browser-log.txt > /home/user/browser-crash-log.txt
  n=$((n+1))
done
