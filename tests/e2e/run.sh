#!/bin/bash
# End-to-end suite for the morph nginx module. Runs INSIDE the docker-morph_build image:
#
#   MSYS_NO_PATHCONV=1 docker run --rm --name morph_e2e \
#     -v "<repo>/morph_module_src:/svc/src/morph_module_src" \
#     -v "<repo>/docker/docker_build.sh:/repo_build.sh:ro" \
#     -v "<repo>/tests:/tests" docker-morph_build:latest \
#     bash -c "sed 's/\r$//' /tests/e2e/run.sh > /tmp/run.sh && bash /tmp/run.sh"
#
# Env: SKIP_BUILD=1 (module already installed), NET=0 (skip internet tests), ONLY=<regex> (filter IDs).
# Prints one PASS/FAIL/SKIP line per scenario and a summary; exits 1 if anything failed.
set -u

E2E=${E2E_DIR:-/tests/e2e}
NGX=/svc/morph/nginx
ROOT=/svc/morph/cache        # servers 8080/8081/8083
ROOT2=/svc/morph/cache2      # server 8082 (different quality / size directives, separate cache)
FX=/tmp/fx
O1=/srv/o9000                # trusted origin, primary copy of every fixture
O2=/srv/o9001                # second origin, only "prefer.png" (green)
OUT=/tmp/out; B=$OUT/body; H=$OUT/hdr
M=http://127.0.0.1:8080/morph      # main: purge on + limit_except, debug on, default limits
M1=http://127.0.0.1:8081/morph     # purge off (default), debug off
M2=http://127.0.0.1:8082/morph     # purge on (no limit_except), quality 20, webp_quality 10, 10..500 x 10..300
M3=http://127.0.0.1:8083/morph     # no morph_thumbnail: plain static location
IP=$(hostname -I | awk '{print $1}')
mkdir -p "$OUT"

# ---------------------------------------------------------------- build
if [ "${SKIP_BUILD:-0}" != 1 ]; then
    echo "== building nginx + morph module (log: /tmp/build.log)"
    (cd /svc/src/nginx-1.26.2 && bash <(sed 's/\r$//' /repo_build.sh)) > /tmp/build.log 2>&1 \
        || { tail -40 /tmp/build.log; echo "BUILD FAILED"; exit 2; }
fi
[ -f $NGX/modules/ngx_http_morph_module.so ] || { echo "module not built"; exit 2; }

# ---------------------------------------------------------------- fixtures
echo "== generating fixtures"
rm -rf "$FX"; python3 "$E2E/fixtures.py" "$FX" || exit 2
cd "$FX"
v() { vips "$@" || { echo "fixture step failed: vips $*"; exit 2; }; }
for n in tb tall lr quad gray gray128 white prefer_green prefer_red; do v copy $n.ppm $n.png; done
v bandjoin "rgba_rgb.ppm rgba_a.pgm" rgba.png
v bandjoin "transp_rgb.ppm transp_a.pgm" transp.png
v bandjoin "wm_rgb.ppm wm_a.pgm" wm.png
v bandjoin "wm_rgb.ppm wmhalf_a.pgm" wmhalf.png
v copy wm_rgb.ppm wmrgb.png
v gifsave anim.ppm anim.gif --page-height 100
v gifsave anim_tb.ppm anim_tb.gif --page-height 100
v gifsave anim_wide.ppm anim_wide.gif --page-height 50
v jpegsave photo.ppm photo.jpg --Q 90
v jpegsave tb.ppm tb.jpg --Q 95
v webpsave tb.ppm tb.webp --Q 95
v gaussblur tb.png soft.png 3
v jpegsave orient_src.ppm orient_plain.jpg --Q 95
python3 "$E2E/fixtures.py" "$FX" exif || exit 2
head -c $(( $(stat -c %s photo.jpg) / 3 )) photo.jpg > truncated.jpg
# 51 distinct 2000x2000 frames = 204 MP (> 200 MP fallback threshold); distinct so cgif keeps every frame
v xyz xyz.v 2000 102000; v extract_band xyz.v y.v 1; v linear y.v f.v 0.0025 0; v cast f.v u.v uchar
v gifsave u.v big.gif --page-height 2000; rm -f xyz.v y.v f.v u.v
[ "$(vipsheader -f orientation orient.jpg)" = 6 ] || { echo "EXIF fixture broken"; exit 2; }
for g in anim anim_tb anim_wide; do [ "$(vipsheader -f n-pages $g.gif)" = 3 ] || { echo "GIF fixture $g broken"; exit 2; }; done
[ "$(vipsheader -f n-pages big.gif)" = 51 ] || { echo "GIF fixture broken"; exit 2; }
# 1100 tiny distinct frames: under 200 MP but over the 1000-frame cap
v xyz xyz.v 8 8800; v extract_band xyz.v y.v 1; v linear y.v f.v 4.625 0; v remainder_const f.v r.v 256; v cast r.v u.v uchar
v gifsave u.v many.gif --page-height 8; rm -f xyz.v y.v f.v r.v u.v
[ "$(vipsheader -f n-pages many.gif)" -gt 1000 ] || { echo "many.gif fixture broken"; exit 2; }
# 16-bit fixtures: opaque red watermark, blue base, mid-grey
v black b.v 40 40 --bands 3; v linear b.v r.v "1 1 1" "65535 0 0"; v cast r.v r16.v ushort; v copy r16.v r16i.v --interpretation rgb16; v pngsave r16i.v wm16.png --bitdepth 16
v black b2.v 100 100 --bands 3; v linear b2.v bl.v "1 1 1" "0 0 65535"; v cast bl.v bl16.v ushort; v copy bl16.v base16.v --interpretation rgb16; v pngsave base16.v base16.png --bitdepth 16
v black g.v 50 50; v linear g.v g2.v 1 32768; v cast g2.v g16.v ushort; v copy g16.v grey16.v --interpretation grey16; v pngsave grey16.v grey16.png --bitdepth 16
rm -f b.v b2.v r.v r16.v r16i.v bl.v bl16.v base16.v g.v g2.v g16.v grey16.v
# 13000x13000 = 169 MP > 150 MP single-frame cap, but a few hundred KB on disk
v black bomb.v 13000 13000; v pngsave bomb.v bomb.png --compression 9; rm -f bomb.v

FILES="tb.png tall.png lr.png quad.png gray.png gray128.png white.png rgba.png transp.png wm.png wmhalf.png wmrgb.png
       anim.gif anim_tb.gif anim_wide.gif big.gif orient.jpg photo.jpg tb.jpg tb.webp soft.png
       notimage.png tiny.png empty.png corrupt.jpg truncated.jpg huge.jpg
       many.gif wm16.png base16.png grey16.png bomb.png"
rm -rf "$ROOT" "$ROOT2" "$O1" "$O2"
mkdir -p "$ROOT/local/sub" "$ROOT/short" "$ROOT/origin" "$ROOT2" "$O1/sub/dir" "$O2"
cp $FILES "$ROOT/local/"; cp $FILES "$O1/"
cp white.png "$ROOT/local/white2.png"; cp wm.png "$ROOT/local/my_wm.png"
cp tb.png "$ROOT/local/sp ace.png"; cp tb.png "$ROOT/local/sub/x.png"; cp tb.png "$ROOT/local/a..b.png"
cp tb.png "$O1/sp ace.png"; cp tb.png "$O1/h#1.png"; cp tb.png "$O1/sub/dir/deep.png"
cp prefer_red.png "$O1/prefer.png"; cp prefer_green.png "$O2/prefer.png"
cp tb.png "$ROOT/short/tb.png"; cp wm.png "$ROOT/origin/wm.png"; cp tb.png "$ROOT/canary.png"
chmod -R a+rwX "$ROOT" "$ROOT2"   # workers run as nobody

# ---------------------------------------------------------------- nginx + origins
cat > $NGX/conf/morph_e2e.json <<'EOF'
{ "service": {
    "local":    { "source": [], "ttl": -1 },
    "short":    { "source": [], "ttl": 1 },
    "origin":   { "source": ["http://127.0.0.1:9000"], "ttl": -1 },
    "failover": { "source": ["http://127.0.0.1:9999", "http://127.0.0.1:9000"], "ttl": -1 },
    "prefer":   { "source": ["http://127.0.0.1:9001", "http://127.0.0.1:9000"] },
    "dead":     { "source": ["http://127.0.0.1:9998", "http://127.0.0.1:9999"], "ttl": -1 },
    "slow":     { "source": ["http://127.0.0.1:9002"], "ttl": -1 },
    "net":      { "source": [], "ttl": -1 }
} }
EOF
cat > $NGX/conf/morph_e2e.conf <<EOF
load_module modules/ngx_http_morph_module.so;
thread_pool pool threads=16 max_queue=65535;
worker_processes 2;
error_log logs/error.log info;
pid logs/nginx.pid;
events { worker_connections 1024; }
http {
    include mime.types;
    default_type application/octet-stream;
    access_log off;
    server {
        listen 8080;
        if (\$request_method = PUT) { return 415; }
        location /morph/ {
            aio threads=pool;
            root $ROOT;
            limit_except GET HEAD { allow 127.0.0.1; deny all; }
            morph_thumbnail;
            morph_debug on;
            morph_purge on;
            quality 90;
            webp_quality 80;
            width_min 1; width_max 4000; height_min 1; height_max 4000;
            service morph_e2e.json;
            error_log logs/errorlog/error.log error;
        }
        error_page 403 404 /40x.html;
        location = /40x.html { root html; }
        error_page 500 502 503 504 /50x.html;
        location = /50x.html { root html; }
    }
    server {
        listen 8081;
        location /morph/ { aio threads=pool; root $ROOT; morph_thumbnail; morph_debug off; service morph_e2e.json; }
    }
    server {
        listen 8082;
        location /morph/ {
            aio threads=pool; root $ROOT2; morph_thumbnail; morph_purge on;
            quality 20; webp_quality 10;
            width_min 10; width_max 500; height_min 10; height_max 300;
            service morph_e2e.json;
        }
    }
    server {
        listen 8083;
        location /morph/ { alias $ROOT/; }
    }
}
EOF
mkdir -p $NGX/logs/accesslog $NGX/logs/errorlog
echo "<html>4xx</html>" > $NGX/html/40x.html; echo "<html>5xx</html>" > $NGX/html/50x.html
# leftovers from an interrupted run in a reused container (no ps/pkill in the image)
[ -f $NGX/logs/nginx.pid ] && kill "$(cat $NGX/logs/nginx.pid)" 2>/dev/null && sleep 1
for p in /proc/[0-9]*; do grep -qa 'http.server\|slow_origin' $p/cmdline 2>/dev/null && kill ${p#/proc/} 2>/dev/null; done
trap 'kill $(jobs -p) 2>/dev/null' EXIT
: > $NGX/logs/error.log; : > $NGX/logs/morph_debug.log; chmod 666 $NGX/logs/morph_debug.log

python3 -m http.server 9000 --bind 127.0.0.1 --directory "$O1" > /tmp/o9000.log 2>&1 &
python3 -m http.server 9001 --bind 127.0.0.1 --directory "$O2" > /tmp/o9001.log 2>&1 &
# slow origin: 1 s then 404, one "HIT <path>" line per request (single-flight error sharing)
cat > /tmp/slow_origin.py <<'PY'
import http.server, sys, time
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        sys.stderr.write("HIT " + self.path + "\n"); sys.stderr.flush(); time.sleep(1)
        self.send_response(404); self.send_header("Content-Length", "0"); self.end_headers()
http.server.ThreadingHTTPServer(("127.0.0.1", 9002), H).serve_forever()
PY
python3 /tmp/slow_origin.py 2> /tmp/o9002.log &
$NGX/sbin/nginx -p $NGX/ -c conf/morph_e2e.conf -t > /tmp/nginx_t.log 2>&1 || { cat /tmp/nginx_t.log; exit 2; }
$NGX/sbin/nginx -p $NGX/ -c conf/morph_e2e.conf || exit 2
for i in $(seq 50); do
    curl -s -o /dev/null http://127.0.0.1:8080/ && curl -s -o /dev/null http://127.0.0.1:9000/ \
        && curl -s -o /dev/null http://127.0.0.1:9001/ && break
    sleep 0.2
done

NET=${NET:-auto}
if [ "$NET" = auto ]; then
    curl -s -m 8 -o /dev/null https://httpbin.org/get && NET=1 || NET=0
fi
echo "== internet tests: $([ "$NET" = 1 ] && echo enabled || echo SKIPPED)"

# ---------------------------------------------------------------- harness
PASS=0; FAIL=0; SKIPN=0; RESULTS=()
get() { U=$1; shift; rm -f "$B" "$H"; CODE=$(curl -sg -o "$B" -D "$H" -w '%{http_code}' --max-time 60 "$@" "$U"); touch "$B" "$H"; }
command -v cmp > /dev/null || cmp() { [ "$(md5sum < "$2")" = "$(md5sum < "$3")" ]; }   # image has no diffutils
hdr() { tr -d '\r' < "$H" | grep -i "^$1:" | tail -1 | cut -d' ' -f2-; }
T() { ID=$1; ERR=""; U=""; RUN=1; [ -n "${ONLY:-}" ] && ! [[ $ID =~ $ONLY ]] && RUN=0; [ $RUN = 1 ]; }
E() { ERR+="$*; "; }
R() {
    [ "$RUN" = 1 ] || return 0
    if [ -z "$ERR" ]; then echo "PASS $ID"; PASS=$((PASS+1)); RESULTS+=("PASS $ID")
    else echo "FAIL $ID :: ${ERR}[last url: $U]"; FAIL=$((FAIL+1)); RESULTS+=("FAIL $ID"); fi
}
S() { echo "SKIP $1 :: $2"; SKIPN=$((SKIPN+1)); RESULTS+=("SKIP $1"); }
net() { [ "$NET" = 1 ]; }
code()   { [ "$CODE" = "$1" ] || E "status $CODE want $1"; }
codein() { [[ " $* " == *" $CODE "* ]] || E "status $CODE want one of: $*"; }
field()  { vipsheader -f "$1" "${2:-$B}" 2>/dev/null; }
isimg()  { vipsheader "${1:-$B}" > /dev/null 2>&1 || E "not a decodable image: ${1:-body}"; }
dim()    { local d; d="$(field width "${2:-$B}")x$(field height "${2:-$B}")"; [ "$d" = "$1" ] || E "dims $d want $1"; }
pages()  { local n; n=$(field n-pages "${2:-$B}"); [ "${n:-1}" = "$1" ] || E "n-pages ${n:-1} want $1"; }
bands()  { local b; b=$(field bands "${2:-$B}"); [ "$b" = "$1" ] || E "bands $b want $1"; }
loader() { local l; l=$(field vips-loader "${2:-$B}"); [[ "$l" == $1* ]] || E "decoded by '$l' want $1*"; }
ctype()  { local c; c=$(hdr Content-Type); [ "$c" = "$1" ] || E "Content-Type '$c' want $1"; }
xcache() { local c; c=$(hdr X-Cache-Status); [ "$c" = "$1" ] || E "X-Cache-Status '$c' want $1"; }
noroot() { ! grep -q "root:" "$B" || E "body contains /etc/passwd content"; }
# px X Y "r g b" [tol]: compares the first 1-3 values of the pixel
# pxp PAGE X Y "r g b" [tol]: same, on GIF/WebP frame PAGE
px()  { _px "$B" "" "$@"; }
pxp() { local p=$1; shift; _px "$B[page=$p]" " page $p" "$@"; }
_px() {
    local got; got=$(vips getpoint "$1" "$3" "$4" 2>/dev/null)
    awk -v g="$got" -v w="$5" -v t="${6:-40}" 'BEGIN{ if (split(g,a," ")<1) exit 1; n=split(w,b," ");
        for (i=1;i<=n;i++){ d=a[i]-b[i]; if (d<0) d=-d; if (d>t) exit 1 } }' \
        || E "pixel($3,$4$2)='$got' want '$5'"
}
# bandv X Y BAND VALUE [tol]: one band of a pixel (e.g. alpha)
bandv() {
    local got; got=$(vips getpoint "$B" "$1" "$2" 2>/dev/null | awk -v i=$(( $3 + 1 )) '{print $i}')
    awk -v g="$got" -v w="$4" -v t="${5:-1}" 'BEGIN{ if (g=="") exit 1; d=g-w; if (d<0) d=-d; exit d>t }' \
        || E "band $3 at ($1,$2)='$got' want $4"
}
keep() { cp "$B" "$OUT/$1"; }
size() { stat -c %s "${1:-$B}"; }
# maxdiff A B: largest absolute per-pixel difference between two same-size images
maxdiff() { vips subtract "$1" "$2" $OUT/d.v && vips abs $OUT/d.v $OUT/a.v && vips max $OUT/a.v; }
# sc ID CODE URL [curl args]: status-only scenario (+ decodable image when 200)
sc() { local id=$1 c=$2 u=$3; shift 3; T "$id" || return 0; get "$u" "$@"; code "$c"; [ "$CODE" = 200 ] && isimg; R; }
rawhead() {  # rawhead PATH -> "<status line>|<body bytes after headers>"
    python3 - "$1" <<'PY'
import socket, sys
s = socket.create_connection(("127.0.0.1", 8080))
s.sendall(("HEAD %s HTTP/1.1\r\nHost: t\r\nConnection: close\r\n\r\n" % sys.argv[1]).encode())
d = b""
while True:
    c = s.recv(65536)
    if not c: break
    d += c
h, _, body = d.partition(b"\r\n\r\n")
print("%s|%d" % (h.split(b"\r\n")[0].decode(), len(body)))
PY
}
RED="255 0 0"; GRN="0 255 0"; BLU="0 0 255"; WHT="255 255 255"

echo "== running scenarios"
# ================================================================ RESIZE
T RESIZE-WIDTH-ONLY && { get "$M/local/200x0/quad.png"; code 200; dim 200x100; ctype image/png; px 50 25 "$RED"; px 150 75 "$WHT"; }; R
T RESIZE-WIDTH-ONLY-PORTRAIT && { get "$M/local/100x0/tall.png"; code 200; dim 100x200; px 50 20 "$RED"; px 50 180 "$BLU"; }; R
T RESIZE-HEIGHT-ONLY-PORTRAIT && { get "$M/local/0x200/tall.png"; code 200; dim 100x200; }; R
T RESIZE-HEIGHT-ONLY && { get "$M/local/0x100/quad.png"; code 200; dim 200x100; px 150 25 "$GRN"; }; R
T RESIZE-COVER-CENTER && { get "$M/local/100x100/quad.png"; code 200; dim 100x100
    px 25 25 "$RED"; px 75 25 "$GRN"; px 25 75 "$BLU"; px 75 75 "$WHT"; }; R
T RESIZE-UPSCALE && { get "$M/local/800x0/quad.png"; code 200; dim 800x400; }; R
T RESIZE-SAME-SIZE && { get "$M/local/400x200/quad.png"; code 200; dim 400x200; px 10 10 "$RED"; }; R
T RESIZE-0x0-NOOP && { get "$M/local/0x0/quad.png"; code 200; dim 400x200; px 390 190 "$WHT"; }; R
T RESIZE-1x1 && { get "$M/local/1x1/quad.png"; code 200; dim 1x1; }; R
T RESIZE-STRETCH-NOT-APPLIED && { get "$M/local/400x100/tb.png"; code 200; dim 400x100; }; R
T RESIZE-JPEG && { get "$M/local/400x300/photo.jpg"; code 200; dim 400x300; loader jpeg; ctype image/jpeg; }; R
T RESIZE-WEBP-SOURCE && { get "$M/local/50x0/tb.webp"; code 200; dim 50x50; loader webp; ctype image/webp; px 25 5 "$RED"; px 25 45 "$BLU"; }; R
T RESIZE-EXIF-AUTOROTATE && { get "$M/local/100x0/orient.jpg"; code 200; dim 100x200; px 50 20 "$RED"; px 50 180 "$BLU"; }; R
T RESIZE-EXIF-NO-RESIZE && { get "$M/local/F(quality:90)/orient.jpg"; code 200; dim 200x400; px 100 20 "$RED"; px 100 380 "$BLU"; }; R
T RESIZE-EXIF-GRAVITY-TOP && { get "$M/local/100x100_G(top)/orient.jpg"; code 200; dim 100x100; px 50 10 "$RED"; px 50 90 "$RED"; }; R
T RESIZE-EXIF-GRAVITY-BOTTOM && { get "$M/local/100x100_G(bottom)/orient.jpg"; code 200; dim 100x100; px 50 10 "$BLU"; px 50 90 "$BLU"; }; R

# ================================================================ GRAVITY (cover crop)
for g in "top RED RED" "north RED RED" "bottom BLU BLU" "south BLU BLU" "center RED BLU" "ctr RED BLU"; do
    set -- $g
    T GRAVITY-${1^^} && { get "$M/local/200x50_G($1)/tb.png"; code 200; dim 200x50; px 100 3 "${!2}"; px 100 46 "${!3}"; }; R
done
T GRAVITY-DEFAULT-IS-CENTER && { get "$M/local/200x50/tb.png"; code 200; px 100 3 "$RED"; px 100 46 "$BLU"; }; R
for g in "left RED" "west RED" "right BLU" "east BLU"; do
    set -- $g
    T GRAVITY-${1^^} && { get "$M/local/50x200_G($1)/lr.png"; code 200; dim 50x200; px 3 100 "${!2}"; px 46 100 "${!2}"; }; R
done
T GRAVITY-OFF-AXIS-FALLS-BACK-TO-CENTER && { get "$M/local/200x50_G(left)/tb.png"; code 200; px 100 3 "$RED"; px 100 46 "$BLU"; }; R
T GRAVITY-AFTER-MANUAL-CROP && { get "$M/local/C0,0,200,200_200x50_G(bottom)/tb.png"; code 200; dim 200x50; px 100 3 "$BLU"; px 100 46 "$BLU"; }; R
T GRAVITY-AFTER-MANUAL-CROP-TOP && { get "$M/local/C0,0,200,200_200x50_G(top)/tb.png"; code 200; px 100 3 "$RED"; px 100 46 "$RED"; }; R

# ================================================================ CROP (coordinates on the original)
T CROP-TOP-LEFT && { get "$M/local/C0,0,200,100/quad.png"; code 200; dim 200x100; px 100 50 "$RED"; px 195 95 "$RED"; }; R
T CROP-BOTTOM-RIGHT && { get "$M/local/C200,100,200,100/quad.png"; code 200; dim 200x100; px 5 5 "$WHT"; }; R
T CROP-MIDDLE && { get "$M/local/C150,50,100,100/quad.png"; code 200; dim 100x100
    px 25 25 "$RED"; px 75 25 "$GRN"; px 25 75 "$BLU"; px 75 75 "$WHT"; }; R
T CROP-CLAMPED-TO-IMAGE && { get "$M/local/C10,10,500,400/quad.png"; code 200; dim 390x190; }; R
T CROP-NEGATIVE-ORIGIN && { get "$M/local/C-5,-5,10,10/quad.png"; code 200; dim 10x10; px 5 5 "$RED"; }; R
T CROP-OUTSIDE-IMAGE && { get "$M/local/C500,500,10,10/quad.png"; code 200; dim 1x1; px 0 0 "$WHT"; }; R
T CROP-THEN-RESIZE-W && { get "$M/local/C200,0,200,100_100x0/quad.png"; code 200; dim 100x50; px 50 25 "$GRN"; }; R
T CROP-THEN-RESIZE-H && { get "$M/local/C200,0,200,100_0x50/quad.png"; code 200; dim 100x50; px 50 25 "$GRN"; }; R
T CROP-THEN-COVER && { get "$M/local/100x100_C0,0,400,200/quad.png"; code 200; dim 100x100; px 25 25 "$RED"; px 75 75 "$WHT"; }; R
T CROP-README-EXAMPLE && { get "$M/local/200x300_C10,10,500,400/photo.jpg"; code 200; dim 200x300; }; R
T CROP-JPEG-ON-ORIGINAL && { get "$M/local/C0,0,400,300/photo.jpg"; code 200; dim 400x300; }; R

# ================================================================ ROTATE / FLIP (quad 400x200: TL red, TR green, BL blue, BR white)
T ROTATE-90 && { get "$M/local/F(rotate:90)/quad.png"; code 200; dim 200x400
    px 50 100 "$BLU"; px 150 100 "$RED"; px 150 300 "$GRN"; px 50 300 "$WHT"; }; R
T ROTATE-180 && { get "$M/local/F(rotate:180)/quad.png"; code 200; dim 400x200
    px 100 50 "$WHT"; px 300 50 "$BLU"; px 100 150 "$GRN"; px 300 150 "$RED"; }; R
T ROTATE-270 && { get "$M/local/F(rotate:270)/quad.png"; code 200; dim 200x400
    px 50 100 "$GRN"; px 150 100 "$WHT"; px 50 300 "$RED"; px 150 300 "$BLU"; }; R
T ROTATE-MINUS-90-EQUALS-270 && { get "$M/local/F(rotate:-90)/quad.png"; code 200; dim 200x400; px 50 100 "$GRN"; px 50 300 "$RED"; }; R
T ROTATE-450-EQUALS-90 && { get "$M/local/F(rotate:450)/quad.png"; code 200; dim 200x400; px 50 100 "$BLU"; px 150 100 "$RED"; }; R
T ROTATE-360-NOOP && { get "$M/local/F(rotate:360)/quad.png"; code 200; dim 400x200; px 10 10 "$RED"; }; R
T ROTATE-45-ARBITRARY && { get "$M/local/F(rotate:45)/quad.png"; code 200; isimg
    w=$(field width); h=$(field height); [ "${w:-0}" -gt 400 ] && [ "${h:-0}" -gt 400 ] || E "45deg canvas ${w}x${h} should exceed 400x400"; }; R
T ROTATE-AFTER-RESIZE && { get "$M/local/200x0_F(rotate:90)/quad.png"; code 200; dim 100x200; px 75 50 "$RED"; }; R
T FLIP-H && { get "$M/local/F(flip:h)/quad.png"; code 200; dim 400x200
    px 100 50 "$GRN"; px 300 50 "$RED"; px 100 150 "$WHT"; px 300 150 "$BLU"; }; R
T FLIP-V && { get "$M/local/F(flip:v)/quad.png"; code 200; px 100 50 "$BLU"; px 300 50 "$WHT"; px 100 150 "$RED"; }; R
T FLIP-DEFAULT-IS-VERTICAL && { get "$M/local/F(flip)/quad.png"; code 200; px 100 50 "$BLU"; px 100 150 "$RED"; }; R
T ROTATE-THEN-FLIP && { get "$M/local/F(rotate:90)_F(flip:h)/quad.png"; code 200; dim 200x400
    px 50 100 "$RED"; px 150 100 "$BLU"; px 50 300 "$GRN"; px 150 300 "$WHT"; }; R

# ================================================================ FILTERS
T FILTER-GRAYSCALE-PNG && { get "$M/local/F(grayscale)/tb.png"; code 200; bands 1; loader png; dim 200x200; }; R
T FILTER-GRAYSCALE-JPEG && { get "$M/local/100x0_F(grayscale)/photo.jpg"; code 200; bands 1; loader jpeg; }; R
T FILTER-BLUR && { get "$M/local/F(blur:10)/tb.png"; code 200; px 100 99 "130 0 125" 50; px 100 2 "$RED" 15; }; R
T FILTER-BLUR-ZERO-IS-NOOP && { get "$M/local/F(blur:0)/tb.png"; code 200; px 100 99 "$RED" 2; }; R
T FILTER-SHARPEN && { get "$M/local/0x0/soft.png"; code 200; keep soft0.png
    get "$M/local/F(sharpen:5)/soft.png"; code 200; dim 200x200; keep soft5.png
    d=$(maxdiff $OUT/soft0.png $OUT/soft5.png); awk -v d="$d" 'BEGIN{exit !(d>5)}' || E "sharpen changed pixels by max $d"; }; R
for b in "2 200" "0.5 50" "0 0" "10 255"; do set -- $b
    T FILTER-BRIGHTNESS-$1 && { get "$M/local/F(brightness:$1)/gray.png"; code 200; px 50 50 "$2 $2 $2" 2; }; R
done
for c in "2 72" "0 128" "0.5 114"; do set -- $c
    T FILTER-CONTRAST-$1 && { get "$M/local/F(contrast:$1)/gray.png"; code 200; px 50 50 "$2 $2 $2" 2; }; R
done
T FILTER-NOISE && { get "$M/local/F(noise:30)/gray128.png"; code 200
    d=$(vips deviate "$B"); a=$(vips avg "$B")
    awk -v d="$d" -v a="$a" 'BEGIN{exit !(d>15 && a>118 && a<138)}' || E "noise deviate=$d avg=$a"; }; R
T FILTER-NOISE-ZERO-IS-NOOP && { get "$M/local/F(noise:0)/gray128.png"; code 200
    d=$(vips deviate "$B"); awk -v d="$d" 'BEGIN{exit !(d<0.5)}' || E "deviate $d"; }; R
T FILTER-BACKGROUND-ON-OPAQUE-JPEG && { get "$M/local/100x0_F(background_color:ff0000)/photo.jpg"; code 200; dim 100x75; }; R
T FILTER-BRIGHTNESS-THEN-CONTRAST && { get "$M/local/F(brightness:2)_F(contrast:2)/gray.png"; code 200; px 50 50 "255 255 255" 2; }; R
T FILTER-README-GRAY-BLUR && { get "$M/local/200x0_F(grayscale)_F(blur:1.5)/quad.png"; code 200; dim 200x100; bands 1; }; R
T FILTER-EVERYTHING-AT-ONCE && {
    get "$M/local/300x200_G(top)_F(grayscale)_F(blur:1)_F(sharpen:1)_F(brightness:1.2)_F(contrast:1.1)_F(noise:5)_F(rotate:90)_F(flip:h)_F(format:webp)_F(quality:50)_W(path:wm.png,g:se,o:0.5)/photo.jpg"
    code 200; loader webp; ctype image/webp; dim 200x300; }; R

# ================================================================ FORMAT / QUALITY
T FORMAT-KEEP-PNG  && { get "$M/local/50x0/tb.png";  code 200; loader png;  ctype image/png; }; R
T FORMAT-KEEP-JPEG && { get "$M/local/50x0/tb.jpg";  code 200; loader jpeg; ctype image/jpeg; }; R
T FORMAT-KEEP-WEBP && { get "$M/local/51x0/tb.webp"; code 200; loader webp; ctype image/webp; }; R
T FORMAT-KEEP-GIF  && { get "$M/local/50x0/anim.gif"; code 200; loader gif; ctype image/gif; }; R
for f in "jpg jpeg image/jpeg" "jpeg jpeg image/jpeg" "png png image/png" "webp webp image/webp" "gif gif image/gif"; do set -- $f
    T FORMAT-TO-${1^^} && { get "$M/local/60x0_F(format:$1)/tb.png"; code 200; loader $2; ctype $3; dim 60x60; px 30 5 "$RED"; px 30 55 "$BLU"; }; R
done
T QUALITY-FILTER-JPEG && { get "$M/local/400x300_F(quality:10)/photo.jpg"; code 200; keep q10.jpg
    get "$M/local/400x300_F(quality:95)/photo.jpg"; code 200
    [ $(size $OUT/q10.jpg) -lt $(( $(size) / 2 )) ] || E "q10 $(size $OUT/q10.jpg)B not < half of q95 $(size)B"; }; R
T QUALITY-FILTER-WEBP && { get "$M/local/400x300_F(format:webp)_F(quality:5)/photo.jpg"; code 200; loader webp; keep w5.webp
    get "$M/local/400x300_F(format:webp)/photo.jpg"; code 200
    [ $(size $OUT/w5.webp) -lt $(size) ] || E "webp q5 $(size $OUT/w5.webp)B not < default $(size)B"; }; R
T QUALITY-DIRECTIVE-JPEG && { get "$M2/origin/400x300/photo.jpg"; code 200; keep d20.jpg
    get "$M/origin/400x300/photo.jpg"; code 200
    [ $(size $OUT/d20.jpg) -lt $(size) ] || E "quality 20 server $(size $OUT/d20.jpg)B not < quality 90 server $(size)B"; }; R
T QUALITY-DIRECTIVE-WEBP && { get "$M2/origin/400x300_F(format:webp)/photo.jpg"; code 200; keep d10.webp
    get "$M/origin/400x300_F(format:webp)/photo.jpg"; code 200
    [ $(size $OUT/d10.webp) -lt $(size) ] || E "webp_quality 10 server $(size $OUT/d10.webp)B not < webp_quality 80 $(size)B"; }; R
T QUALITY-FILTER-OVERRIDES-DIRECTIVE && { get "$M2/origin/400x300_F(quality:95)/photo.jpg"; code 200
    [ $(size) -gt $(size $OUT/d20.jpg) ] || E "F(quality:95) $(size)B not > directive q20 $(size $OUT/d20.jpg)B"; }; R
T QUALITY-FILTER-OVERRIDES-WEBP-DIRECTIVE && { get "$M2/origin/400x300_F(format:webp)_F(quality:95)/photo.jpg"; code 200
    [ $(size) -gt $(size $OUT/d10.webp) ] || E "webp F(quality:95) $(size)B not > webp_quality 10 $(size $OUT/d10.webp)B"; }; R

# ================================================================ GIF (anim.gif: 3 frames 100x100, left half red/green/blue, right white)
gif_frames() {  # gif_frames X Y : frame k at (X,Y) must be colour k
    local k c; for k in 0 1 2; do c=(RED GRN BLU); pxp $k "$1" "$2" "${!c[$k]}"; done
}
T GIF-PASSTHROUGH && { get "$M/local/F(format:gif)/anim.gif"; code 200; loader gif; pages 3; dim 100x100; gif_frames 25 50; pxp 1 75 50 "$WHT"; }; R
T GIF-RESIZE && { get "$M/local/50x0/anim.gif"; code 200; pages 3; dim 50x50; gif_frames 12 25; pxp 2 40 25 "$WHT"; }; R
T GIF-RESIZE-HEIGHT && { get "$M/local/0x40/anim.gif"; code 200; pages 3; dim 40x40; gif_frames 8 20; }; R
T GIF-COVER && { get "$M/local/50x30/anim.gif"; code 200; pages 3; dim 50x30; gif_frames 10 15; }; R
T GIF-GRAVITY-TOP && { get "$M/local/100x25_G(top)/anim_tb.gif"; code 200; pages 3; dim 100x25
    for k in 0 1 2; do pxp $k 50 3 "$RED"; pxp $k 50 22 "$RED"; done; }; R
T GIF-GRAVITY-BOTTOM && { get "$M/local/100x25_G(bottom)/anim_tb.gif"; code 200; pages 3
    for k in 0 1 2; do pxp $k 50 3 "$BLU"; pxp $k 50 22 "$BLU"; done; }; R
T GIF-CROP && { get "$M/local/C0,0,50,50/anim.gif"; code 200; pages 3; dim 50x50; gif_frames 25 25; }; R
T GIF-CROP-THEN-RESIZE && { get "$M/local/C0,0,50,50_20x20/anim.gif"; code 200; pages 3; dim 20x20; gif_frames 10 10; }; R
T GIF-ROTATE-90 && { get "$M/local/F(rotate:90)/anim_wide.gif"; code 200; pages 3; dim 50x100; gif_frames 25 25; pxp 0 25 75 "$WHT"; }; R
T GIF-ROTATE-45 && { get "$M/local/F(rotate:45)/anim.gif"; code 200; pages 3; }; R
T GIF-FLIP-H && { get "$M/local/F(flip:h)/anim.gif"; code 200; pages 3; dim 100x100; gif_frames 75 50; pxp 1 25 50 "$WHT"; }; R
T GIF-FLIP-V && { get "$M/local/F(flip:v)/anim_tb.gif"; code 200; pages 3; for k in 0 1 2; do pxp $k 50 10 "$BLU"; done; }; R
T GIF-BLUR-NO-FRAME-BLEED && { get "$M/local/F(blur:5)/anim.gif"; code 200; pages 3; pxp 1 10 0 "$GRN" 30; pxp 1 10 99 "$GRN" 30; pxp 2 10 0 "$BLU" 30; }; R
T GIF-SHARPEN && { get "$M/local/F(sharpen:2)/anim.gif"; code 200; pages 3; gif_frames 10 50; }; R
T GIF-GRAYSCALE && { get "$M/local/F(grayscale)/anim.gif"; code 200; pages 3; }; R
T GIF-BRIGHTNESS-NOISE && { get "$M/local/F(brightness:1.2)_F(noise:5)/anim.gif"; code 200; pages 3; }; R
T GIF-WATERMARK && { get "$M/local/W(path:wm.png,g:se)/anim.gif"; code 200; pages 3; for k in 0 1 2; do pxp $k 90 90 "$RED"; pxp $k 75 10 "$WHT"; done; }; R
T GIF-TO-WEBP-ANIMATED && { get "$M/local/F(format:webp)/anim.gif"; code 200; loader webp; ctype image/webp; pages 3; dim 100x100; }; R
T GIF-TO-JPG-SINGLE-FRAME && { get "$M/local/F(format:jpg)/anim.gif"; code 200; loader jpeg; ctype image/jpeg; dim 100x100; }; R
T GIF-TO-PNG-SINGLE-FRAME && { get "$M/local/50x0_F(format:png)/anim.gif"; code 200; loader png; dim 50x50; }; R
T GIF-OVER-200MP-STATIC-FALLBACK && { get "$M/local/100x0/big.gif"; code 200; loader gif; pages 1; dim 100x100; }; R
T GIF-OVER-200MP-NO-RESIZE && { get "$M/local/F(format:gif)/big.gif"; code 200; pages 1; dim 2000x2000; }; R

# ================================================================ WATERMARK (white 200x200 base, wm.png 50x50 opaque red)
for w in "nw 25 25" "n 100 25" "ne 175 25" "w 25 100" "c 100 100" "e 175 100" "sw 25 175" "s 100 175" "se 175 175" \
         "tl 25 25" "top 100 25" "tr 175 25" "left 25 100" "center 100 100" "right 175 100" "bl 25 175" "bottom 100 175" "br 175 175"; do
    set -- $w
    T WATERMARK-GRAVITY-${1^^} && { get "$M/local/W(path:wm.png,g:$1)/white.png"; code 200; dim 200x200; px $2 $3 "$RED"
        if [ $2 = 100 ] && [ $3 = 100 ]; then px 10 10 "$WHT"; else px 100 100 "$WHT"; fi; }; R
done
T WATERMARK-DEFAULT-CENTER && { get "$M/local/W(path:wm.png)/white.png"; code 200; px 100 100 "$RED"; px 10 10 "$WHT"; }; R
T WATERMARK-OPACITY-HALF && { get "$M/local/W(path:wm.png,g:se,o:0.5)/white.png"; code 200; px 175 175 "255 128 128" 20; }; R
T WATERMARK-OPACITY-LONG-KEY && { get "$M/local/W(path:wm.png,g:se,opacity:0.5)/white.png"; code 200; px 175 175 "255 128 128" 20; }; R
T WATERMARK-OPACITY-ZERO && { get "$M/local/W(path:wm.png,g:se,o:0)/white.png"; code 200; px 175 175 "$WHT" 3; }; R
T WATERMARK-OFFSET && { get "$M/local/W(path:wm.png,g:nw,x:20,y:30)/white.png"; code 200
    px 10 10 "$WHT"; px 25 35 "$RED"; px 65 75 "$RED"; px 75 85 "$WHT"; }; R
T WATERMARK-NEGATIVE-OFFSET && { get "$M/local/W(path:wm.png,g:se,x:-10,y:-10)/white.png"; code 200; px 195 195 "$WHT"; px 185 185 "$RED"; px 145 145 "$RED"; }; R
T WATERMARK-OWN-ALPHA && { get "$M/local/W(path:wmhalf.png,g:c)/white.png"; code 200; px 85 100 "$WHT"; px 115 100 "$RED"; }; R
T WATERMARK-NO-ALPHA-SOURCE && { get "$M/local/W(path:wmrgb.png,g:c)/white.png"; code 200; px 100 100 "$RED"; bands 3; }; R
T WATERMARK-AFTER-RESIZE && { get "$M/local/100x100_W(path:wm.png,g:se)/white.png"; code 200; dim 100x100; px 90 90 "$RED"; px 40 40 "$WHT"; }; R
T WATERMARK-ON-GRAYSCALE && { get "$M/local/F(grayscale)_W(path:wm.png,g:c)/white.png"; code 200; isimg; dim 200x200; }; R
T WATERMARK-ON-RGBA-BASE && { get "$M/local/W(path:wm.png,g:c)/rgba.png"; code 200; bands 4; bandv 5 5 3 128; }; R
T WATERMARK-LARGER-THAN-BASE && { get "$M/local/20x20_W(path:wm.png,g:c)/white.png"; code 200; dim 20x20; px 10 10 "$RED"; }; R
T WATERMARK-FAR-OFFSET && { get "$M/local/W(path:wm.png,x:100000,y:100000)/white.png"; code 200; dim 200x200; px 100 100 "$WHT"; }; R
# a missing watermark must not be served (and cached) as an unwatermarked image
T WATERMARK-MISSING-FILE-404 && { get "$M/local/W(path:nofile.png,g:c)/white.png"; code 404; }; R
T WATERMARK-UNDERSCORE-IN-FILENAME && { get "$M/local/W(path:my_wm.png,g:c)/white.png"; code 200; px 100 100 "$RED"; }; R
T WATERMARK-ORIGIN-SERVICE && { get "$M/origin/W(path:wm.png,g:nw)/white.png"; code 200; px 25 25 "$RED"; px 100 100 "$WHT"; }; R
T WATERMARK-JPEG-BASE && { get "$M/local/200x150_W(path:wm.png,g:c)/photo.jpg"; code 200; loader jpeg; px 100 75 "$RED"; }; R

# ================================================================ ALPHA (rgba.png: 100x100 (200,50,50) alpha 128)
T ALPHA-RESIZE-KEEPS-ALPHA && { get "$M/local/50x0/rgba.png"; code 200; bands 4; bandv 25 25 3 128 2; }; R
T ALPHA-BRIGHTNESS && { get "$M/local/F(brightness:1.5)/rgba.png"; code 200; bands 4; px 50 50 "255 75 75" 2; bandv 50 50 3 128; }; R
T ALPHA-CONTRAST && { get "$M/local/F(contrast:1.5)/rgba.png"; code 200; bands 4; px 50 50 "236 11 11" 2; bandv 50 50 3 128; }; R
T ALPHA-NOISE && { get "$M/local/F(noise:20)/rgba.png"; code 200; bands 4
    vips extract_band "$B" $OUT/al.v 3 && mn=$(vips min $OUT/al.v) && mx=$(vips max $OUT/al.v)
    awk -v a="$mn" -v b="$mx" 'BEGIN{exit !(a==128 && b==128)}' || E "alpha range after noise ${mn:-?}..${mx:-?}"; }; R
T ALPHA-GRAYSCALE && { get "$M/local/F(grayscale)/rgba.png"; code 200; bands 2; bandv 50 50 1 128; }; R
T ALPHA-BACKGROUND-COLOR && { get "$M/local/F(background_color:00ff00)/transp.png"; code 200; bands 3; px 25 50 "$GRN" 2; px 75 50 "$GRN" 2; }; R
T ALPHA-BACKGROUND-COLOR-RED && { get "$M/local/F(background_color:FF0000)/transp.png"; code 200; px 25 50 "$RED" 2; px 75 50 "$GRN" 2; }; R
T ALPHA-BACKGROUND-COLOR-TYPO-ALIAS && { get "$M/local/F(bakground_color:0000ff)/transp.png"; code 200; px 25 50 "$BLU" 2; }; R
T ALPHA-TO-WEBP && { get "$M/local/F(format:webp)/rgba.png"; code 200; loader webp; bands 4; }; R
T ALPHA-TO-JPEG && { get "$M/local/F(format:jpg)/rgba.png"; code 200; loader jpeg; bands 3; }; R

# ================================================================ CACHE
T CACHE-MISS-THEN-HIT && { get "$M/local/70x0/tb.png"; code 200; xcache MISS; keep c1
    get "$M/local/70x0/tb.png"; code 200; xcache HIT; cmp -s $OUT/c1 "$B" || E "HIT body differs from MISS body"; }; R
T CACHE-HIT-CONTENT-TYPE-PNG && { get "$M/local/70x0/tb.png"; xcache HIT; ctype image/png; }; R
T CACHE-HIT-CONTENT-TYPE-WEBP && { get "$M/local/70x0_F(format:webp)/tb.png"; get "$M/local/70x0_F(format:webp)/tb.png"; xcache HIT; ctype image/webp; loader webp; }; R
T CACHE-HIT-CONTENT-TYPE-GIF && { get "$M/local/70x0/anim.gif"; get "$M/local/70x0/anim.gif"; xcache HIT; ctype image/gif; pages 3; }; R
T CACHE-HIT-CONTENT-TYPE-JPEG && { get "$M/local/70x0/tb.jpg"; get "$M/local/70x0/tb.jpg"; xcache HIT; ctype image/jpeg; }; R
T CACHE-FILE-LOCATION-LOCAL && { get "$M/local/100x0_F(grayscale)/tb.png"; code 200; f="$ROOT/local/100x0_F%28grayscale%29@q90-80/tb.png"
    [ -f "$f" ] || E "missing cache file $f"; cmp -s "$f" "$B" || E "cache file != body"; }; R
T CACHE-FILE-LOCATION-ORIGIN && { get "$M/origin/30x0/sub/dir/deep.png"; code 200; f="$ROOT/origin/30x0@q90-80/sub/dir/deep.png"
    [ -f "$f" ] || E "missing cache file $f"; get "$M/origin/30x0/sub/dir/deep.png"; xcache HIT; }; R
T CACHE-FILE-MODE-READABLE && { m=$(stat -c %a "$ROOT/local/70x0@q90-80/tb.png" 2>/dev/null); [ "$m" = 644 ] || E "cache file mode $m"; }; R
T CACHE-QUERY-STRING-IGNORED && { get "$M/local/70x0/tb.png?v=123"; code 200; xcache HIT; }; R
T CACHE-SERVES-STALE-WHEN-NO-TTL && { get "$M/origin/71x0/prefer.png"; code 200; xcache MISS
    cp "$FX/prefer_green.png" "$O1/prefer.png"; get "$M/origin/71x0/prefer.png"; xcache HIT; px 10 10 "$RED" 5
    cp "$FX/prefer_red.png" "$O1/prefer.png"; }; R
T CACHE-TTL-NEVER-EXPIRES && { get "$M/local/72x0/tb.png"; touch -d '30 days ago' "$ROOT/local/72x0@q90-80/tb.png"
    get "$M/local/72x0/tb.png"; code 200; xcache HIT; }; R
T CACHE-TTL-EXPIRY && { get "$M/short/50x0/tb.png"; code 200; xcache MISS; get "$M/short/50x0/tb.png"; xcache HIT
    sleep 3; get "$M/short/50x0/tb.png"; code 200; xcache MISS; isimg; get "$M/short/50x0/tb.png"; xcache HIT; }; R
T CACHE-LAST-MODIFIED-ON-MISS && { get "$M/local/73x0/tb.png"; xcache MISS; [ -n "$(hdr Last-Modified)" ] || E "no Last-Modified on MISS"; }; R
T CACHE-304-IF-MODIFIED-SINCE && { get "$M/local/73x0/tb.png"; xcache HIT; lm=$(hdr Last-Modified)
    [ -n "$lm" ] || E "no Last-Modified on HIT"; get "$M/local/73x0/tb.png" -H "If-Modified-Since: $lm"; code 304
    [ "$(size)" = 0 ] || E "304 carried a body of $(size) bytes"; }; R
T CACHE-200-IF-MODIFIED-SINCE-OLD && { get "$M/local/73x0/tb.png" -H "If-Modified-Since: Mon, 01 Jan 1990 00:00:00 GMT"; code 200; isimg; }; R
T CACHE-CONCURRENT-IDENTICAL && {
    U="$M/local/123x0_F(blur:3)_F(sharpen:2)/photo.jpg"
    pids=(); for i in $(seq 1 50); do curl -sg -o $OUT/cc$i -w '%{http_code}' "$U" > $OUT/cc$i.code & pids+=($!); done; wait "${pids[@]}"
    for i in $(seq 1 50); do
        [ "$(cat $OUT/cc$i.code)" = 200 ] || E "req $i status $(cat $OUT/cc$i.code)"
        [ "$(field width $OUT/cc$i)" = 123 ] || E "req $i invalid image"
    done
    left=$(find "$ROOT" -regextype posix-extended -regex '.*\.[A-Za-z0-9]{6}$' | head -5)
    [ -z "$left" ] || E "temp files left: $left"; }; R
T CACHE-CONCURRENT-SINGLE-FLIGHT && {
    U="$M/local/124x0_F(blur:8)/big.gif"
    pids=(); for i in $(seq 1 20); do curl -sg -o $OUT/sf$i -D $OUT/sf$i.h -w '%{http_code}' "$U" > $OUT/sf$i.code & pids+=($!); done; wait "${pids[@]}"
    for i in $(seq 1 20); do [ "$(cat $OUT/sf$i.code)" = 200 ] || E "req $i status $(cat $OUT/sf$i.code)"; done
    m=$(cat $OUT/sf*.h | tr -d '\r' | grep -ci '^X-Cache-Status: MISS')
    [ "$m" = 1 ] || E "$m of 20 concurrent identical misses rendered (want 1)"
    left=$(find "$ROOT" -path '*/@lock/*' -type f | head -3); [ -z "$left" ] || E "lock files left: $left"; }; R
T CACHE-CONCURRENT-ERROR-NO-LOCK-LEFT && {
    pids=(); for i in $(seq 1 10); do curl -sg -o /dev/null -w '%{http_code}' "$M/local/125x0/missing.png" > $OUT/se$i.code & pids+=($!); done; wait "${pids[@]}"
    for i in $(seq 1 10); do [ "$(cat $OUT/se$i.code)" = 404 ] || E "req $i status $(cat $OUT/se$i.code)"; done
    left=$(find "$ROOT" -path '*/@lock/*' -type f | head -3); [ -z "$left" ] || E "lock files left: $left"; }; R
T CACHE-CONCURRENT-ERROR-SHARED && {
    pids=(); for i in $(seq 1 10); do curl -sg -o /dev/null -w '%{http_code}' "$M/slow/126x0/gone.png" > $OUT/sg$i.code & pids+=($!); done; wait "${pids[@]}"
    for i in $(seq 1 10); do [ "$(cat $OUT/sg$i.code)" = 404 ] || E "req $i status $(cat $OUT/sg$i.code)"; done
    n=$(grep -c 'HIT /gone.png' /tmp/o9002.log); [ "$n" = 1 ] || E "slow origin fetched $n times for 10 concurrent requests (want 1)"
    get "$M/slow/126x0/gone.png"; code 404; n=$(grep -c 'HIT /gone.png' /tmp/o9002.log)
    [ "$n" = 2 ] || E "a later request must retry the source, origin saw $n fetches"; }; R
T CACHE-LOCK-DIR-NOT-A-SOURCE && { mkdir -p "$ROOT/local/@lock"; cp "$FX/tb.png" "$ROOT/local/@lock/planted.png"
    get "$M/local/50x0/@lock/planted.png"; c1=$CODE; get "$M/local/50x0/./@lock/planted.png" --path-as-is; c2=$CODE
    rm -f "$ROOT/local/@lock/planted.png"; [ "$c1" = 404 ] && [ "$c2" = 404 ] || E "lock folder served as a source ($c1, $c2)"; }; R
T CACHE-ERRORS-NOT-CACHED && { get "$M/local/74x0/notimage.png"; code 415; get "$M/local/74x0/missing.png"; code 404
    [ ! -e "$ROOT/local/74x0@q90-80/notimage.png" ] && [ ! -e "$ROOT/local/74x0@q90-80/missing.png" ] || E "error response left a cache file"; }; R
T CACHE-KEY-NO-COLLISION-WATERMARK && {
    get "$M/local/W(path:wm.png:g:se)/white2.png"
    get "$M/local/W(path:wm.png,g:se)/white2.png"; code 200; px 175 175 "$RED"
    [ "$(hdr X-Cache-Status)" = HIT ] && E "served HIT from the cache entry of a different option string W(path:wm.png:g:se)"; }; R

# ================================================================ PURGE
T PURGE-LOCAL && { get "$M/local/80x0/tb.png"; f="$ROOT/local/80x0@q90-80/tb.png"; [ -f "$f" ] || E "not cached"
    get "$M/local/80x0/tb.png" -X DELETE; code 204; [ ! -e "$f" ] || E "cache file still there"; }; R
T PURGE-AGAIN-404 && { get "$M/local/80x0/tb.png" -X DELETE; code 404; }; R
T PURGE-THEN-MISS && { get "$M/local/80x0/tb.png"; code 200; xcache MISS; }; R
T PURGE-ORIGIN-SUBDIR && { get "$M/origin/80x0/sub/dir/deep.png"; f="$ROOT/origin/80x0@q90-80/sub/dir/deep.png"; [ -f "$f" ] || E "not cached"
    get "$M/origin/80x0/sub/dir/deep.png" -X DELETE; code 204; [ ! -e "$f" ] || E "cache file still there"; }; R
T PURGE-SOURCE-FILE-UNTOUCHED && { [ -f "$ROOT/local/tb.png" ] || E "local source was deleted"; }; R
T PURGE-OFF-405 && { get "$M1/local/81x0/tb.png"; code 200; f="$ROOT/local/81x0@q90-80/tb.png"
    get "$M1/local/81x0/tb.png" -X DELETE; code 405; [ -f "$f" ] || E "cache file removed"; }; R
T PURGE-LIMIT-EXCEPT-DENIES-OTHER-IP && { get "http://$IP:8080/morph/local/81x0/tb.png" -X DELETE --interface "$IP"; code 403
    [ -f "$ROOT/local/81x0@q90-80/tb.png" ] || E "cache file removed"; }; R
T PURGE-LIMIT-EXCEPT-GET-FROM-OTHER-IP-OK && { get "http://$IP:8080/morph/local/81x0/tb.png" --interface "$IP"; code 200; xcache HIT; }; R
T PURGE-LIMIT-EXCEPT-ALLOWS-LOCALHOST && { get "$M/local/81x0/tb.png" -X DELETE; code 204; [ ! -e "$ROOT/local/81x0@q90-80/tb.png" ] || E "not removed"; }; R
T PURGE-WITHOUT-LIMIT-EXCEPT && { get "$M2/origin/82x0/tb.png"; code 200; [ -f "$ROOT2/origin/82x0@q20-10/tb.png" ] || E "not cached"
    get "$M2/origin/82x0/tb.png" -X DELETE; code 204; [ ! -e "$ROOT2/origin/82x0@q20-10/tb.png" ] || E "not removed"; }; R
T PURGE-BAD-OPTIONS-400 && { get "$M/local/zzz/tb.png" -X DELETE; code 400; }; R
T PURGE-TRAVERSAL-BLOCKED && { get "$M/local/0x0/..%2f..%2fcanary.png" -X DELETE --path-as-is; [ "$CODE" = 204 ] && E "DELETE returned 204"
    get "$M/local/0x0/x/%2e%2e/%2e%2e/%2e%2e/canary.png" -X DELETE --path-as-is; [ "$CODE" = 204 ] && E "DELETE returned 204"
    [ -f "$ROOT/canary.png" ] || E "canary deleted"; }; R

# ================================================================ SOURCES
T SOURCE-LOCAL && { get "$M/local/50x0/lr.png"; code 200; dim 50x50; px 10 25 "$RED"; }; R
T SOURCE-LOCAL-SUBDIR && { get "$M/local/50x0/sub/x.png"; code 200; dim 50x50; }; R
T SOURCE-LOCAL-SPACE-IN-NAME && { get "$M/local/50x0/sp%20ace.png"; code 200; dim 50x50; }; R
T SOURCE-LOCAL-MISSING-404 && { get "$M/local/50x0/missing.png"; code 404; }; R
T SOURCE-ORIGIN && { get "$M/origin/50x0/lr.png"; code 200; dim 50x50; px 40 25 "$BLU"; }; R
T SOURCE-ORIGIN-SUBDIR && { get "$M/origin/50x0/sub/dir/deep.png"; code 200; dim 50x50; }; R
T SOURCE-ORIGIN-MISSING-404 && { get "$M/origin/50x0/missing.png"; code 404; }; R
T SOURCE-ORIGIN-SPACE-IN-NAME && { get "$M/origin/50x0/sp%20ace.png"; code 200; dim 50x50; }; R
T SOURCE-ORIGIN-HASH-IN-NAME && { get "$M/origin/50x0/h%231.png"; code 200; dim 50x50; }; R
T SOURCE-FAILOVER-DEAD-PRIMARY && { get "$M/failover/50x0/tb.png"; code 200; dim 50x50; px 25 5 "$RED"; }; R
T SOURCE-FAILOVER-PRIMARY-404 && { get "$M/prefer/50x0/tb.png"; code 200; dim 50x50; }; R
T SOURCE-PRIMARY-PREFERRED && { get "$M/prefer/20x0/prefer.png"; code 200; px 10 10 "$GRN" 5; }; R
T SOURCE-ALL-ORIGINS-DEAD-404 && { get "$M/dead/50x0/tb.png"; code 404; }; R
T SOURCE-UNKNOWN-SERVICE-404 && { get "$M/nosuchservice/50x0/tb.png"; code 404; }; R
T SOURCE-OVER-50MB-LOCAL && { get "$M/local/50x0/huge.jpg"; codein 404 413; }; R
T SOURCE-OVER-50MB-ORIGIN && { get "$M/origin/50x0/huge.jpg"; codein 404 413; }; R
T SOURCE-TRUSTED-ORIGIN-NOT-SSRF-CHECKED && { get "$M/origin/F(format:png)/tb.png"; code 200; }; R
if net; then
    T SOURCE-URL-HTTP && { get "$M/net/50x0/http:/httpbin.org/image/png"; code 200; loader png; ctype image/png; [ "$(field width)" = 50 ] || E "width $(field width)"; }; R
    T SOURCE-URL-HTTPS-JPEG && { get "$M/net/50x0/https:/httpbin.org/image/jpeg"; code 200; loader jpeg; [ "$(field width)" = 50 ] || E "width"; }; R
    T SOURCE-URL-HTTPS-WEBP && { get "$M/net/50x0/https:/httpbin.org/image/webp"; code 200; loader webp; ctype image/webp; }; R
    T SOURCE-URL-DOUBLE-SLASH && { get "$M/net/51x0/https://httpbin.org/image/png"; code 200; loader png; }; R
    T SOURCE-URL-REDIRECT-FOLLOWED && { get "$M/net/0x0/https:/picsum.photos/200/300"; code 200; dim 200x300; }; R
    T SOURCE-URL-HTTP-404 && { get "$M/net/50x0/https:/httpbin.org/status/404"; code 404; }; R
    T SOURCE-URL-NON-IMAGE-415 && { get "$M/net/50x0/https:/httpbin.org/html"; code 415; }; R
    T SOURCE-URL-CACHE-HIT-CONTENT-TYPE && { get "$M/net/52x0/https:/httpbin.org/image/png"; xcache MISS
        f="$ROOT/net/52x0@q90-80/$(printf '%s' 'https:/httpbin.org/image/png' | md5sum | cut -d' ' -f1).jpg"
        [ -f "$f" ] || E "cache file $f missing"; get "$M/net/52x0/https:/httpbin.org/image/png"; xcache HIT; ctype image/png; }; R
    T PURGE-URL-SOURCE && { f="$ROOT/net/52x0@q90-80/$(printf '%s' 'https:/httpbin.org/image/png' | md5sum | cut -d' ' -f1).jpg"
        get "$M/net/52x0/https:/httpbin.org/image/png" -X DELETE; code 204; [ ! -e "$f" ] || E "not removed"; }; R
    T PURGE-URL-SOURCE-WITH-FORMAT && { get "$M/net/53x0_F(format:webp)/https:/httpbin.org/image/png"; code 200
        f="$ROOT/net/53x0_F%28format%3Awebp%29@q90-80/$(printf '%s' 'https:/httpbin.org/image/png' | md5sum | cut -d' ' -f1).webp"
        [ -f "$f" ] || E "cache file $f missing"; get "$M/net/53x0_F(format:webp)/https:/httpbin.org/image/png" -X DELETE; code 204
        [ ! -e "$f" ] || E "not removed"; }; R
else
    for s in SOURCE-URL-HTTP SOURCE-URL-HTTPS-JPEG SOURCE-URL-HTTPS-WEBP SOURCE-URL-DOUBLE-SLASH SOURCE-URL-REDIRECT-FOLLOWED \
             SOURCE-URL-HTTP-404 SOURCE-URL-NON-IMAGE-415 SOURCE-URL-CACHE-HIT-CONTENT-TYPE PURGE-URL-SOURCE PURGE-URL-SOURCE-WITH-FORMAT; do S $s offline; done
fi

# ================================================================ SECURITY: SSRF (each target really serves an image if reachable)
while read -r id url; do
    T "$id" && { get "$M/net/50x0/$url"; code 403; }; R
done <<EOF
SSRF-LOOPBACK http:/127.0.0.1:9000/tb.png
SSRF-LOOPBACK-HTTPS-SCHEME https:/127.0.0.1:9000/tb.png
SSRF-LOOPBACK-SHORT http:/127.1:9000/tb.png
SSRF-DECIMAL-IP http:/2130706433:9000/tb.png
SSRF-HEX-IP http:/0x7f.1:9000/tb.png
SSRF-OCTAL-IP http:/0177.0.0.1:9000/tb.png
SSRF-ZERO-ADDR http:/0.0.0.0:9000/tb.png
SSRF-IPV6-LOOPBACK http:/[::1]:9000/tb.png
SSRF-IPV4-MAPPED-IPV6 http:/[::ffff:127.0.0.1]:9000/tb.png
SSRF-IPV4-MAPPED-IPV6-HEX http:/[::ffff:7f00:1]:9000/tb.png
SSRF-NAT64-LOOPBACK http:/[64:ff9b::7f00:1]:9000/tb.png
SSRF-IPV6-ULA http:/[fd00::1]/x.png
SSRF-METADATA http:/169.254.169.254/latest/meta-data/
SSRF-10-NET http:/10.0.0.1/x.png
SSRF-172-16 http:/172.16.0.1/x.png
SSRF-172-31 http:/172.31.255.254/x.png
SSRF-DOCKER-SELF-IP http:/$IP:8080/morph/local/0x0/tb.png
SSRF-192-168 http:/192.168.1.1/x.png
SSRF-CGNAT-100-64 http:/100.64.0.1/x.png
SSRF-MULTICAST http:/224.0.0.1/x.png
EOF
if net; then
    T SSRF-DNS-NAME-TO-LOOPBACK && { get "$M/net/50x0/http:/localtest.me:9000/tb.png"; code 403; }; R
    # httpbin.org/redirect-to refuses non-httpbin targets (403), postman-echo.com is an open redirector
    T SSRF-REDIRECT-CONTROL-PUBLIC && { get "$M/net/50x0/https:/postman-echo.com/redirect-to%3Furl=https%253A%252F%252Fhttpbin.org%252Fimage%252Fpng"; code 200; loader png; }; R
    T SSRF-REDIRECT-TO-LOOPBACK && { get "$M/net/50x0/https:/postman-echo.com/redirect-to%3Furl=http%253A%252F%252F127.0.0.1%253A9000%252Ftb.png"; code 403; }; R
    T SSRF-REDIRECT-TO-METADATA && { get "$M/net/50x0/https:/postman-echo.com/redirect-to%3Furl=http%253A%252F%252F169.254.169.254%252Flatest"; code 403; }; R
else
    for s in SSRF-DNS-NAME-TO-LOOPBACK SSRF-REDIRECT-CONTROL-PUBLIC SSRF-REDIRECT-TO-LOOPBACK SSRF-REDIRECT-TO-METADATA; do S $s offline; done
fi
T SSRF-FILE-SCHEME-NOT-FETCHED && { get "$M/net/0x0/file:/etc/passwd"; [ "$CODE" = 200 ] && E "200"; noroot; }; R

# ================================================================ SECURITY: path traversal / service
while read -r id path; do
    T "$id" && { get "http://127.0.0.1:8080$path" --path-as-is; [ "$CODE" = 200 ] && E "status 200"; noroot; }; R
done <<'EOF'
TRAVERSAL-DOTDOT /morph/local/0x0/../../../../../etc/passwd
TRAVERSAL-ENCODED-DOTS /morph/local/0x0/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/etc/passwd
TRAVERSAL-ENCODED-SLASH /morph/local/0x0/..%2f..%2f..%2f..%2f..%2fetc%2fpasswd
TRAVERSAL-DOUBLE-ENCODED /morph/local/0x0/%252e%252e%252f%252e%252e%252f%252e%252e%252fetc%252fpasswd
TRAVERSAL-IN-SERVICE /morph/..%2f..%2f..%2f..%2fetc/0x0/passwd
TRAVERSAL-ORIGIN-SERVICE /morph/origin/0x0/..%2f..%2f..%2f..%2fetc%2fpasswd
TRAVERSAL-NUL-BYTE /morph/local/0x0/tb.png%00.jpg
EOF
T TRAVERSAL-TO-ROOT-CANARY && { get "$M/local/0x0/..%2fcanary.png" --path-as-is; [ "$CODE" = 200 ] && E "read a file outside the service dir"; }; R
T TRAVERSAL-DOTDOT-IN-NAME-403 && { get "$M/local/50x0/a..b.png"; code 403; }; R

# ================================================================ LIMITS (default server: 1..4000)
while read -r id c opts; do sc "$id" "$c" "$M/local/$opts/gray.png"; done <<'EOF'
LIMIT-WIDTH-MAX 200 4000x0
LIMIT-WIDTH-OVER-MAX 400 4001x0
LIMIT-HEIGHT-MAX 200 0x4000
LIMIT-HEIGHT-OVER-MAX 400 0x4001
LIMIT-WIDTH-NEGATIVE 400 -1x10
LIMIT-HEIGHT-NEGATIVE 400 10x-1
LIMIT-DIM-MISSING-HEIGHT 400 100x
LIMIT-DIM-GARBAGE 400 100xabc
LIMIT-DIM-OVERFLOW 400 99999999999x1
LIMIT-DIM-OVERFLOW-H 400 1x99999999999
LIMIT-BLUR-0 200 F(blur:0)
LIMIT-BLUR-50 200 F(blur:50)
LIMIT-BLUR-OVER 400 F(blur:50.1)
LIMIT-BLUR-NEGATIVE 400 F(blur:-1)
LIMIT-BLUR-ABC 400 F(blur:abc)
LIMIT-BLUR-NAN 400 F(blur:nan)
LIMIT-BLUR-INF 400 F(blur:inf)
LIMIT-BLUR-1E999 400 F(blur:1e999)
LIMIT-BLUR-EMPTY 400 F(blur:)
LIMIT-BLUR-NO-VALUE 400 F(blur)
LIMIT-SHARPEN-10 200 F(sharpen:10)
LIMIT-SHARPEN-OVER 400 F(sharpen:10.1)
LIMIT-SHARPEN-ABC 400 F(sharpen:abc)
LIMIT-BRIGHTNESS-10 200 F(brightness:10)
LIMIT-BRIGHTNESS-OVER 400 F(brightness:10.5)
LIMIT-BRIGHTNESS-NEGATIVE 400 F(brightness:-0.1)
LIMIT-BRIGHTNESS-NAN 400 F(brightness:nan)
LIMIT-CONTRAST-10 200 F(contrast:10)
LIMIT-CONTRAST-OVER 400 F(contrast:11)
LIMIT-CONTRAST-INF 400 F(contrast:inf)
LIMIT-NOISE-100 200 F(noise:100)
LIMIT-NOISE-OVER 400 F(noise:100.1)
LIMIT-NOISE-NEGATIVE 400 F(noise:-1)
LIMIT-QUALITY-1 200 F(quality:1)
LIMIT-QUALITY-100 200 F(quality:100)
LIMIT-QUALITY-0 400 F(quality:0)
LIMIT-QUALITY-101 400 F(quality:101)
LIMIT-QUALITY-NEGATIVE 400 F(quality:-5)
LIMIT-QUALITY-ABC 400 F(quality:abc)
LIMIT-QUALITY-OVERFLOW 400 F(quality:99999999999)
LIMIT-ROTATE-3600 200 F(rotate:3600)
LIMIT-ROTATE-MINUS-3600 200 F(rotate:-3600)
LIMIT-ROTATE-OVER 400 F(rotate:3601)
LIMIT-ROTATE-ABC 400 F(rotate:abc)
LIMIT-ROTATE-NAN 400 F(rotate:nan)
LIMIT-ROTATE-1E308 400 F(rotate:1e308)
LIMIT-FLIP-BAD 400 F(flip:x)
LIMIT-FORMAT-BMP 400 F(format:bmp)
LIMIT-FORMAT-TIFF 400 F(format:tiff)
LIMIT-FORMAT-EMPTY 400 F(format:)
LIMIT-BGCOLOR-OK 200 F(background_color:FF00ff)
LIMIT-BGCOLOR-SHORT 400 F(background_color:fff)
LIMIT-BGCOLOR-LONG 400 F(background_color:1234567)
LIMIT-BGCOLOR-NON-HEX 400 F(background_color:gggggg)
LIMIT-FILTER-UNKNOWN 400 F(sepia)
LIMIT-FILTER-EMPTY 400 F()
LIMIT-FILTER-UNCLOSED 400 F(blur:1
LIMIT-WM-OPACITY-1 200 W(path:wm.png,o:1)
LIMIT-WM-OPACITY-OVER 400 W(path:wm.png,o:1.5)
LIMIT-WM-OPACITY-NEGATIVE 400 W(path:wm.png,o:-0.1)
LIMIT-WM-OPACITY-NAN 400 W(path:wm.png,o:nan)
LIMIT-WM-OPACITY-ABC 400 W(path:wm.png,o:abc)
LIMIT-WM-X-GARBAGE 400 W(path:wm.png,x:zz)
LIMIT-WM-Y-GARBAGE 400 W(path:wm.png,y:zz)
LIMIT-WM-X-OVERFLOW 400 W(path:wm.png,x:99999999999)
LIMIT-WM-GRAVITY-BAD 400 W(path:wm.png,g:zz)
LIMIT-WM-UNKNOWN-KEY 400 W(path:wm.png,foo:1)
LIMIT-WM-NO-PATH 400 W(g:c)
LIMIT-WM-PATH-DOT 400 W(path:.)
LIMIT-WM-PATH-DOTDOT 400 W(path:..)
LIMIT-WM-PATH-DOTS 400 W(path:...)
LIMIT-WM-ITEM-WITHOUT-COLON 400 W(path:wm.png,gse)
LIMIT-WM-EMPTY 400 W()
LIMIT-CROP-GARBAGE 400 Cabc
LIMIT-CROP-THREE-VALUES 400 C1,2,3
LIMIT-GRAVITY-BAD 400 G(xyz)
LIMIT-GRAVITY-EMPTY 400 G()
LIMIT-UNKNOWN-PREFIX-X 400 X100
LIMIT-UNKNOWN-PREFIX-WORD 400 abc
LIMIT-UNKNOWN-PREFIX-Z 400 100x100_Z(1)
EOF
# directive-driven limits (8082: width 10..500, height 10..300)
while read -r id c opts; do sc "$id" "$c" "$M2/origin/$opts/gray.png"; done <<'EOF'
DIRECTIVE-WIDTH-MAX 200 500x0
DIRECTIVE-WIDTH-OVER-MAX 400 501x0
DIRECTIVE-WIDTH-MIN 200 10x0
DIRECTIVE-WIDTH-UNDER-MIN 400 9x0
DIRECTIVE-HEIGHT-MAX 200 0x300
DIRECTIVE-HEIGHT-OVER-MAX 400 0x301
DIRECTIVE-HEIGHT-MIN 200 0x10
DIRECTIVE-HEIGHT-UNDER-MIN 400 0x9
DIRECTIVE-ZERO-MEANS-AUTO 200 0x0
EOF

# ================================================================ PROTOCOL
T PROTOCOL-HEAD-MISS-NO-BODY && { r=$(rawhead "/morph/local/90x0/tb.png"); [[ "$r" == "HTTP/1.1 200"*"|0" ]] || E "raw HEAD -> '$r'"
    get "$M/local/91x0/tb.png" -I; code 200; ctype image/png; xcache MISS;
    [ "${hdr_cl:=$(hdr Content-Length)}" -gt 0 ] 2>/dev/null || E "Content-Length '$(hdr Content-Length)'"; }; R
T PROTOCOL-HEAD-HIT-NO-BODY && { r=$(rawhead "/morph/local/90x0/tb.png"); [[ "$r" == "HTTP/1.1 200"*"|0" ]] || E "raw HEAD -> '$r'"
    get "$M/local/91x0/tb.png" -I; xcache HIT; hl=$(hdr Content-Length)
    get "$M/local/91x0/tb.png"; [ "$hl" = "$(size)" ] || E "HEAD Content-Length $hl != GET body $(size)"; }; R
T PROTOCOL-CONTENT-LENGTH-MATCHES && { get "$M/local/92x0/tb.png"; [ "$(hdr Content-Length)" = "$(size)" ] || E "CL $(hdr Content-Length) body $(size)"; }; R
sc PROTOCOL-POST-405 405 "$M/local/50x0/tb.png" -X POST -d x=1
sc PROTOCOL-PUT-415 415 "$M/local/50x0/tb.png" -X PUT -d x=1
sc PROTOCOL-OPTIONS-405 405 "$M/local/50x0/tb.png" -X OPTIONS
sc PROTOCOL-PATCH-405 405 "$M/local/50x0/tb.png" -X PATCH -d x=1
sc PROTOCOL-MALFORMED-NO-SERVICE 400 "$M/"
sc PROTOCOL-MALFORMED-SERVICE-ONLY 400 "$M/local"
sc PROTOCOL-MALFORMED-SERVICE-SLASH 400 "$M/local/"
sc PROTOCOL-MALFORMED-NO-SOURCE 400 "$M/local/100x0"
T PROTOCOL-EMPTY-SOURCE-NOT-204 && { get "$M/origin/34x0/"; codein 400 404 415; }; R
T PROTOCOL-DIRECTORY-SOURCE && { get "$M/origin/35x0/sub"; codein 404 415; }; R
T CACHE-SOURCE-NAME-EQUALS-CACHE-SUBDIR && { get "$M/origin/36x0/sub/dir/deep.png"; code 200   # creates cache dir .../36x0/sub/
    get "$M/origin/36x0/sub"; codein 404 415; }; R
sc PROTOCOL-NON-IMAGE-415 415 "$M/local/50x0/notimage.png"
sc PROTOCOL-NON-IMAGE-ORIGIN-415 415 "$M/origin/50x0/notimage.png"
sc PROTOCOL-TINY-FILE-415 415 "$M/local/50x0/tiny.png"
T PROTOCOL-EMPTY-FILE && { get "$M/local/50x0/empty.png"; codein 404 415; }; R
T PROTOCOL-CORRUPT-JPEG-NO-CRASH && { get "$M/local/50x0/corrupt.jpg"; codein 415 500; }; R
T PROTOCOL-TRUNCATED-JPEG-NO-CRASH && { get "$M/local/50x0/truncated.jpg"; codein 200 415 500; }; R
T PROTOCOL-CORS-HEADER-UNTOUCHED && { get "$M/local/50x0/tb.png"; code 200; [ -n "$(hdr Last-Modified)" ] || E "no Last-Modified"; }; R

# ================================================================ DIRECTIVES
T DIRECTIVE-MORPH-DEBUG-ON && { get "$M/local/93x0/tb.png"; code 200; sleep 0.3
    grep -q "Request URL: /morph/local/93x0/tb.png" $NGX/logs/morph_debug.log || E "no debug log line"; }; R
T DIRECTIVE-MORPH-DEBUG-OFF && { get "$M1/local/94x0/tb.png"; code 200; sleep 0.3
    ! grep -q "94x0" $NGX/logs/morph_debug.log || E "debug lines written with morph_debug off"; }; R
T DIRECTIVE-NO-MORPH-THUMBNAIL-STATIC && { get "$M3/local/tb.png"; code 200; cmp -s "$B" "$ROOT/local/tb.png" || E "not the raw file"
    get "$M3/local/77x0/tb.png"; code 404; }; R   # 77x0 is never requested through morph, so no cache file

# ================================================================ ROBUSTNESS (nasty inputs must not 5xx or kill a worker)
LONG=$(printf 'F(grayscale)_%.0s' $(seq 1 300))
while read -r id opts; do
    T "$id" && { get "$M/local/$opts/tb.png"; codein 200 400 403 404 415; }; R
done <<EOF
ROBUST-UNCLOSED-F F(
ROBUST-UNCLOSED-W W(
ROBUST-UNCLOSED-G G(
ROBUST-BARE-C C
ROBUST-ONLY-UNDERSCORES ___
ROBUST-TRAILING-UNDERSCORE 100x100_
ROBUST-INT-MAX-DIMS 2147483647x2147483647
ROBUST-CROP-INT-MAX C2147483647,2147483647,2147483647,2147483647
ROBUST-CROP-OFFSET-PLUS-INT-MAX C1,1,2147483647,2147483647
ROBUST-CROP-INT-MIN C-2147483648,-2147483648,10,10
ROBUST-CROP-ZERO C0,0,0,0
ROBUST-CROP-NEGATIVE-SIZE C0,0,-1,-1
ROBUST-WM-INT-MAX-OFFSET W(path:wm.png,x:2147483647,y:2147483647)
ROBUST-WM-INT-MIN-OFFSET W(path:wm.png,g:se,x:-2147483648,y:-2147483648)
ROBUST-TINY-BLUR F(blur:0.0000001)
ROBUST-TINY-SHARPEN F(sharpen:0.0000001)
ROBUST-TINY-NOISE F(noise:0.0000001)
ROBUST-TINY-ROTATE F(rotate:0.000001)
ROBUST-TINY-BRIGHTNESS F(brightness:0.0000001)
ROBUST-LONG-OPTIONS $LONG
ROBUST-REPEATED-FILTER F(rotate:45)_F(rotate:90)
EOF
T ROBUST-GIF-CROP-OVERFLOW && { get "$M/local/C1,1,2147483647,2147483647/anim.gif"; codein 200 400; }; R
# ================================================================ REGRESSIONS (verification findings)
T GIF-FRAME-CAP-FALLBACK && { get "$M/local/100x100_F(rotate:45)/many.gif"; code 200; pages 1; }; R
T GIF-FRAME-CAP-FLIP && { get "$M/local/F(flip)/many.gif"; code 200; pages 1; }; R
T WATERMARK-16BIT-OPAQUE && { get "$M/local/W(path:wm16.png,g:c)/base16.png"; code 200; px 50 50 "$RED"; px 2 2 "$BLU"; }; R
T WATERMARK-16BIT-ON-8BIT && { get "$M/local/W(path:wm16.png,g:c)/white.png"; code 200; px 100 100 "$RED"; }; R
T FILTER-CONTRAST-16BIT-PIVOT && { get "$M/local/F(contrast:0.5)/grey16.png"; code 200; px 10 10 "32768" 300; }; R
T FILTER-BACKGROUND-GREY-ALPHA && { get "$M/local/F(background_color:ff0000)/transp.png"; code 200; isimg; }; R
T LIMIT-DECOMPRESSION-BOMB-413 && { get "$M/local/100x100/bomb.png"; code 413; }; R
T LIMIT-DECOMPRESSION-BOMB-NORESIZE-413 && { get "$M/local/F(grayscale)/bomb.png"; code 413; }; R
T PROTOCOL-EMPTY-SOURCE-400 && { get "$M/local/100x0/"; code 400; }; R
T SOURCE-ORIGIN-QUERY-NOT-INJECTED && { get "$M/origin/50x0/tb.png%3Fa=1"; code 404; }; R
T ROBUST-REQUEST-BODY-NOT-A-REQUEST && {
    n=$(python3 - <<'PY'
import socket
inner = b"GET /morph/local/51x0/tb.png HTTP/1.1\r\nHost: t\r\n\r\n"
req = (b"GET /morph/local/50x0/tb.png HTTP/1.1\r\nHost: t\r\nContent-Length: %d\r\n\r\n" % len(inner)) + inner
s = socket.create_connection(("127.0.0.1", 8080)); s.settimeout(3); s.sendall(req)
d = b""
try:
    while True:
        c = s.recv(65536)
        if not c: break
        d += c
except socket.timeout:
    pass
print(d.count(b"HTTP/1.1 "))
PY
); [ "$n" = 1 ] || E "$n responses for one request (body was parsed as a request)"; }; R

T ROBUST-NO-WORKER-CRASH && { sleep 0.5; c=$(grep -E "exited on signal|exited with code [1-9]|core dumped" $NGX/logs/error.log)
    [ -z "$c" ] || E "$(echo "$c" | head -3)"; curl -s -o /dev/null -w '%{http_code}' "$M/local/50x0/tb.png" | grep -q 200 || E "server not answering"; }; R
T ROBUST-NO-TEMP-FILE-LEFTOVERS && { left=$(find "$ROOT" "$ROOT2" -regextype posix-extended -regex '.*\.[A-Za-z0-9]{6}$' | head -5)
    [ -z "$left" ] || E "leftover temp files: $left"; }; R

# ---------------------------------------------------------------- summary
echo
echo "================ SUMMARY ================"
printf '%s\n' "${RESULTS[@]}" | awk '{printf "%-5s %s\n", $1, $2}' > $OUT/results.txt
grep -c . $OUT/results.txt > /dev/null
echo "total=$((PASS+FAIL+SKIPN)) passed=$PASS failed=$FAIL skipped=$SKIPN"
[ $FAIL -gt 0 ] && { echo "failed:"; grep '^FAIL' $OUT/results.txt; }
cp $OUT/results.txt /tests/e2e/last_results.txt 2>/dev/null
[ "${KEEP:-0}" = 1 ] && { echo "KEEP=1: nginx left running"; sleep infinity; }
$NGX/sbin/nginx -p $NGX/ -c conf/morph_e2e.conf -s quit 2>/dev/null
[ $FAIL -eq 0 ]
