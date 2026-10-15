#pragma once

// Trang cài WiFi (SetupPortal). Gọi /scan, /save, /status.
inline constexpr char kSetupPortalPage[] = R"html(<!doctype html>
<html lang="vi"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Cài WiFi cho mạch</title>
<style>
:root{--bg:#f4f6f8;--card:#fff;--text:#1b1f24;--muted:#5f6b7a;--line:#d8dee6;--accent:#0b6bcb;--ok:#1a7f37;--err:#c62828}
@media (prefers-color-scheme:dark){:root{--bg:#111418;--card:#1b2027;--text:#e6e9ee;--muted:#9aa5b4;--line:#2d3540;--accent:#4c9bf0;--ok:#4ac26b;--err:#ff6b6b}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,sans-serif}
main{max-width:440px;margin:0 auto;padding:16px}
h1{font-size:20px;margin:8px 0 4px}p{margin:4px 0 12px;color:var(--muted)}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin-bottom:12px}
ul{list-style:none;margin:0;padding:0;max-height:260px;overflow:auto}
li{display:flex;justify-content:space-between;gap:8px;padding:10px 8px;border-bottom:1px solid var(--line);cursor:pointer;border-radius:6px}
li:hover,li.sel{background:color-mix(in srgb,var(--accent) 14%,transparent)}
li span:last-child{color:var(--muted);font-size:13px;white-space:nowrap}
label{display:block;font-size:14px;color:var(--muted);margin:10px 0 4px}
input{width:100%;padding:10px;border:1px solid var(--line);border-radius:8px;background:transparent;color:var(--text);font-size:16px}
.row{display:flex;gap:8px;align-items:center;margin-top:6px;font-size:14px;color:var(--muted)}.row input{width:auto}
button{width:100%;margin-top:14px;padding:12px;border:0;border-radius:8px;background:var(--accent);color:#fff;font-size:16px;font-weight:600;cursor:pointer}
button.sec{background:transparent;color:var(--accent);border:1px solid var(--line);margin-top:8px;padding:8px}
button:disabled{opacity:.6}
#st{font-weight:600}.ok{color:var(--ok)}.err{color:var(--err)}
</style></head><body><main>
<h1>Cài WiFi cho mạch</h1>
<p>Chọn WiFi nhà rồi nhập mật khẩu. Mạch chỉ dùng WiFi 2.4GHz.</p>
<div class="card">
<ul id="list"><li>Đang tìm WiFi…</li></ul>
<button class="sec" id="rescan" type="button">Tìm lại</button>
</div>
<form class="card" id="f">
<label for="ssid">Tên WiFi</label><input id="ssid" maxlength="32" required autocomplete="off">
<label for="pw">Mật khẩu</label><input id="pw" type="password" maxlength="64" autocomplete="off">
<div class="row"><input type="checkbox" id="show"><label for="show" style="margin:0">Hiện mật khẩu</label></div>
<button id="go">Kết nối</button>
<p id="st"></p>
</form>
</main><script>
const $=id=>document.getElementById(id);
function bars(r){return r>-55?'▂▄▆█':r>-67?'▂▄▆':r>-78?'▂▄':'▂'}
async function scan(){
  try{const d=await (await fetch('/scan')).json();
    if(d.scanning){setTimeout(scan,1500)}
    const l=$('list');l.innerHTML='';
    if(!d.networks.length){l.innerHTML='<li>'+(d.scanning?'Đang tìm WiFi…':'Không thấy WiFi nào')+'</li>';return}
    d.networks.forEach(n=>{const li=document.createElement('li');
      const a=document.createElement('span');a.textContent=n.ssid;
      const b=document.createElement('span');b.textContent=(n.secure?'🔒 ':'')+bars(n.rssi);
      li.append(a,b);li.onclick=()=>{document.querySelectorAll('li').forEach(x=>x.classList.remove('sel'));li.classList.add('sel');$('ssid').value=n.ssid;$('pw').focus()};
      l.append(li)})
  }catch(e){setTimeout(scan,2000)}
}
$('rescan').onclick=()=>{fetch('/scan?refresh=1').then(()=>setTimeout(scan,500))};
$('show').onchange=e=>$('pw').type=e.target.checked?'text':'password';
function status(t,c){const s=$('st');s.textContent=t;s.className=c||''}
async function poll(){
  try{const d=await (await fetch('/status')).json();
    if(d.state==='connected'){status('Đã vào WiFi nhà. Địa chỉ của mạch: http://'+d.ip+'  — nối laptop/điện thoại về WiFi nhà rồi mở địa chỉ này. WiFi cài đặt sẽ tự tắt sau ít phút.','ok');$('go').disabled=false;return}
    if(d.state==='failed'){status('Không vào được WiFi. Kiểm tra mật khẩu rồi thử lại.','err');$('go').disabled=false;return}
  }catch(e){}
  setTimeout(poll,1500)
}
$('f').onsubmit=async e=>{e.preventDefault();
  const pw=$('pw').value;if(pw&&pw.length<8){status('Mật khẩu WiFi phải từ 8 ký tự','err');return}
  $('go').disabled=true;status('Đang kết nối… (tối đa ~15 giây)');
  const body=new URLSearchParams({ssid:$('ssid').value,password:pw});
  try{const r=await fetch('/save',{method:'POST',body});const d=await r.json();
    if(!d.ok){status(d.error||'Lỗi','err');$('go').disabled=false;return}
  }catch(e){}
  setTimeout(poll,1500)
};
scan();
</script></body></html>)html";
