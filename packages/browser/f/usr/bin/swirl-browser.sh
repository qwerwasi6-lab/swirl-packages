#!/usr/bin/dash
# r182 (browser 1.4.0): "swirl-browser.sh --app NAME INST URL" runs a Web App (swirl-webapp: one
# site, no browser bar) on its own X display :INST, so it can run next to the browser.
APP=; INST=0
if [ "$1" = "--app" ]; then APP=$2; INST=$3; shift 3; fi
export SWIRL_INST=$INST
export GIO_MODULE_DIR=/usr/lib/x86_64-linux-gnu/gio/modules SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt LIBGL_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri GBM_BACKENDS_PATH=/usr/lib/x86_64-linux-gnu/gbm LP_NUM_THREADS=0 DISPLAY=:$INST HOME=/home/user WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1 GDK_BACKEND=x11 NO_AT_BRIDGE=1 GSETTINGS_BACKEND=memory JSC_usePollingTraps=true JSC_useConcurrentJIT=false
# r127: draw web pages on the GPU (Mesa crocus through /dev/dri) when SwirlOS offers one.
# Touch /home/user/.browser-software to go back to the old software renderer.
if [ -e /dev/dri/renderD128 ] && [ ! -e /home/user/.browser-software ]; then
  export WEBKIT_FORCE_COMPOSITING_MODE=1 WEBKIT_FORCE_DMABUF_RENDERER=1 WEBKIT_DMABUF_RENDERER_FORCE_SHM=1 WEBKIT_WEB_RENDER_DEVICE_FILE=/dev/dri/renderD128 LIBGL_DRI3_DISABLE=1 LIBGL_DRI2_DISABLE=1
  echo "renderer: GPU" > /system/debug/browser-renderer.txt
else
  export MESA_LOADER_DRIVER_OVERRIDE=swrast LIBGL_ALWAYS_SOFTWARE=1 WEBKIT_DISABLE_DMABUF_RENDERER=1 WEBKIT_DISABLE_COMPOSITING_MODE=1
  echo "renderer: software" > /system/debug/browser-renderer.txt
fi
# r177: video + sound (Media Codecs package): GStreamer scans its plugins in-process (no helper
# process), keeps its registry in the home folder and plays sound through /dev/dsp.
export GST_REGISTRY_FORK=no GST_PLUGIN_SYSTEM_PATH=/usr/lib/x86_64-linux-gnu/gstreamer-1.0 GST_REGISTRY=/home/user/.cache/gstreamer-registry.bin GST_PLUGIN_FEATURE_RANK=osssink:MAX ORC_CODE=backup
printf 'multi on\n' > /etc/host.conf
printf 'passwd: files\ngroup: files\nhosts: files dns\n' > /etc/nsswitch.conf
# r155: the X screen is as big as the SwirlOS display, so the browser window can grow to fill a
# maximised window (xbridge resizes it to whatever size the SwirlOS window has).
SZ=$(/usr/bin/cat /home/user/.lx/screen 2> /dev/null)
[ -n "$SZ" ] || SZ=1366x768
try=0
while [ $try -lt 4 ]; do
  /usr/bin/Xvfb :$INST +iglx -screen 0 ${SZ}x24 -nolisten tcp -noreset > /system/debug/browser-xvfb-log$INST.txt 2>&1 &
  XPID=$!
  n=0
  while [ $n -lt 40 ]; do
    if /usr/bin/xdpyinfo > /dev/null 2>&1; then break 2; fi
    kill -0 $XPID 2> /dev/null || break
    /usr/bin/sleep 1; n=$((n+1))
  done
  try=$((try+1))
done
# browser 1.7.0: one clipboard for SwirlOS and this X server (copy here, paste in SwirlOS and back)
/usr/bin/swirl-clipd :$INST 2> /system/debug/clipd$INST.txt &
( while :; do /usr/bin/xbridge /tmp/.X11-unix/X$INST >> /tmp/xb$INST.log 2>&1; /usr/bin/sleep 2; done ) &
/usr/bin/sleep 1
# browser 1.6.3: DNS over HTTPS helper for SwirlOS's DNS stub (r194): networks that never answer
# some plain DNS questions (Discord's remote-auth-gateway.discord.gg) still get answers over HTTPS.
/usr/bin/swirl-doh 2> /system/debug/doh-log$INST.txt &
# r123: if the browser itself dies, keep its log and start it again (5 tries)
# browser 1.5.0: the JavaScript JIT is on (the "hang" of 1.4.1 was SwirlOS's storage bug, fixed
# in r185). If the engine dies, the next tries fall back to the interpreter.
# /home/user/.browser-nojit turns it off for good.
n=0
while [ $n -lt 5 ]; do
  J=true
  if [ $n -gt 0 ] || [ -e /home/user/.browser-nojit ]; then J=false; fi
  if [ -n "$APP" ]; then
    # browser 1.7.1: the JIT is on for web apps too (the 1.5.1 "stall" was the storage bug and
    # the console pipe, both fixed). A crash restarts it on the interpreter; touch
    # /home/user/.webapp-nojit to keep web apps on the interpreter for good.
    # browser 1.7.3: back OFF by default (Discord stopped opening with it on real hardware);
    # touch /home/user/.webapp-jit to try it.
    WJ=false; [ $n -eq 0 ] && [ -e /home/user/.webapp-jit ] && WJ=true
    JSC_useJIT=$WJ /usr/bin/swirl-webapp "$APP" "$1" > /system/debug/webapp-$APP-log.txt 2>&1
    # browser 1.7.0: a clean exit (the window was closed) is not a crash - don't start it again
    [ $? -eq 0 ] && break
  else
    # browser 1.7.2: keep cookies on disk (MiniBrowser kept them in memory only: signed out every restart)
    JSC_useJIT=$J /usr/lib/x86_64-linux-gnu/webkit2gtk-4.1/MiniBrowser --cookies-file=/home/user/.browser-cookies.sqlite --cookies-policy=always "${1:-file:///usr/share/swirl-home.html}" > /system/debug/browser-log.txt 2>&1
    /usr/bin/cat /system/debug/browser-log.txt > /system/debug/browser-crash-log.txt
  fi
  n=$((n+1))
done
